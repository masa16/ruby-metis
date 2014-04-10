/*
  metis.c
  METIS API wrapper for Ruby
    (C) Copyright 2011 by Masahiro TANAKA

  This program is free software.
  You can distribute/modify this program
  under the same terms as MIT License.
  NO WARRANTY.
*/
#include <math.h>
#include <ruby.h>
#include "metis.h"

#ifdef IDXTYPE_INT
#define IDXTYPEWIDTH 32
#endif

#if IDXTYPEWIDTH==32

#if   SIZEOF_LONG==4
#define NUM2IDXTYPE(x) NUM2LONG(x)
#define IDXTYPE2NUM(x) LONG2NUM(x)
#define IDXFMT "l"
#elif SIZEOF_INT==4
#define NUM2IDXTYPE(x) NUM2INT(x)
#define IDXTYPE2NUM(x) UINT2NUM(x)
#define IDXFMT ""
#else
#error "IDXTYPEWIDTH==32 is not available for Ruby"
#endif

#elif IDXTYPEWIDTH==64

#if   SIZEOF_LONG==8
#define NUM2IDXTYPE(x) NUM2LONG(x)
#define IDXTYPE2NUM(x) LONG2NUM(x)
#define IDXFMT "l"
#elif SIZEOF_LONG_LONG==8
#define NUM2IDXTYPE(x) NUM2LL(x)
#define IDXTYPE2NUM(x) LL2NUM(x)
#define IDXFMT "ll"
#else
#error "IDXTYPEWIDTH==64 is not available for Ruby"
#endif

#else
  #error "Incorrect user-supplied value fo IDXTYPEWIDTH"
#endif


struct GraphData {
    idxtype *xadj;
    idxtype *adjncy;
    idxtype *vwgt;
    idxtype *adjwgt;
    float   *tpwgts;
    float   *ubvec;
    idxtype *part;
};


static void
g_free(struct GraphData *g)
{
    if (g->xadj) xfree(g->xadj);
    if (g->adjncy) xfree(g->adjncy);
    if (g->vwgt) xfree(g->vwgt);
    if (g->adjwgt) xfree(g->adjwgt);
    if (g->tpwgts) xfree(g->tpwgts);
    if (g->ubvec) xfree(g->ubvec);
    if (g->part) xfree(g->part);
    xfree(g);
}

static void
g_debug_print(struct GraphData *g, idxtype n, idxtype ncon)
{
    idxtype i, j, nadj, nall;

    if (g->xadj) {
	printf("xadj=[");
	for (i=0; i<=n; i++) {
	    printf("%"IDXFMT"d,",g->xadj[i]);
	}
	printf("]\n");
    }

    if (g->adjncy) {
	nadj = g->xadj[n];
	printf("adjncy=[");
	for (i=0,j=1; i<nadj; i++) {
	    printf("%"IDXFMT"d,",g->adjncy[i]);
	    if (g->xadj[j]-1==i) {printf(" "); j++;}
	}
	printf("]\n");
    }

    if (g->vwgt) {
	nall = n*ncon;
	printf("vwgt=[");
	for (i=0; i<n; i++) {
	    for (j=0; j<ncon; j++) {
		printf("%"IDXFMT"d,",g->vwgt[i*ncon+j]);
	    }
	    printf(" ");
	}
	printf("]\n");
    }
    /*
    if (g->adjwgt) xfree(g->adjwgt);
    if (g->tpwgts) xfree(g->tpwgts);
    if (g->ubvec) xfree(g->ubvec);
    if (g->part) xfree(g->part);
    xfree(g);
    */
}


#define check_array(a,n)			\
    if (TYPE(a)!=T_ARRAY) {			\
	if (err_str_len > 0) {			\
	    err_str[err_str_len++] = ',';	\
	}					\
	strncpy(&err_str[err_str_len++], n, 1);	\
    }						\

#define check_array2(a,n)			\
    if (TYPE(a)!=T_ARRAY && !NIL_P(a)) {	\
	if (err_str_len > 0) {			\
	    err_str[err_str_len++] = ',';	\
	}					\
	strncpy(&err_str[err_str_len++], n, 1);	\
    }						\


#define get_idxary(dst,src,n)				\
    dst = ALLOC_N(idxtype,n);				\
    for (i=0; i<(n); i++) {				\
	(dst)[i] = NUM2IDXTYPE(RARRAY_PTR(src)[i]);	\
    }


static void
get_partwgt(float *a, VALUE rbary, long n)
{
    long i;
    float w, sum;

    sum = 0;
    for (i=0; i<n; i++) {
	a[i] = w = (float)NUM2DBL(RARRAY_PTR(rbary)[i]);
	sum += w;
    }
    if (fabs(sum-1.0) > 2.4e-7*n) {
	for (i=0; i<n; i++) {
	    a[i] /= sum;
	}
    }
}


static VALUE
metis_part_graph_main(VALUE v_xadj,
		      VALUE v_adjncy,
		      VALUE v_vwgt,
		      VALUE v_adjwgt,
		      VALUE v_tpwgts,
		      int flag)
{
    idxtype i;
    idxtype n_vertex;
    idxtype n_edges2;
    idxtype n_parts;

    idxtype wgtflag = 0;
    idxtype numflag = 0;

    idxtype options[5] = {0,0,0,0,0};
    idxtype edgecut = 0;

    void (*func)();

    struct GraphData *g;
    volatile VALUE v;
    volatile VALUE v_part;

    char err_str[10] = "";
    int err_str_len = 0;

    // check argument type
    check_array(v_xadj,   "1");
    check_array(v_adjncy, "2");
    check_array2(v_vwgt,  "3");
    check_array2(v_adjwgt,"4");

    if (err_str_len > 0) {
	err_str[err_str_len] = '\0';
	rb_raise(rb_eArgError,"[%s]-th arguments are not an Array",err_str);
    }

    n_vertex = RARRAY_LEN(v_xadj);
    n_edges2 = RARRAY_LEN(v_adjncy);

    // check array sizes
    if (!NIL_P(v_vwgt)) {
	if (RARRAY_LEN(v_vwgt)+1 != n_vertex) {
	    rb_raise(rb_eArgError,"xadj.length(=%"IDXFMT"d) != vwgt.length(=%ld)+1",
		     n_vertex, RARRAY_LEN(v_vwgt) );
	}
	wgtflag = 2;
    }

    if (!NIL_P(v_adjwgt)) {
	if (RARRAY_LEN(v_adjwgt) != n_edges2) {
	    rb_raise(rb_eArgError,"adjncy.length(=%"IDXFMT"d) != adjwgt.length(=%ld)",
		     n_edges2, RARRAY_LEN(v_adjwgt) );
	}
	wgtflag |= 1;
    }

    // wrap as a Ruby-object to be GC-ed automatically
    g = ALLOC(struct GraphData);
    MEMZERO(g,struct GraphData,1);
    v = Data_Wrap_Struct(rb_cData,0,g_free,g);

    // allocate array
    get_idxary(g->xadj,v_xadj,n_vertex);
    n_vertex -= 1;
    get_idxary(g->adjncy,v_adjncy,n_edges2);

    if (wgtflag & 2) {
	get_idxary(g->vwgt,v_vwgt,n_vertex);
    }
    if (wgtflag & 1) {
	get_idxary(g->adjwgt,v_adjwgt,n_edges2);
    }

    // result array
    g->part = ALLOC_N(idxtype,n_vertex);

    // partition weight
    if (TYPE(v_tpwgts) == T_ARRAY) {
	n_parts = RARRAY_LEN(v_tpwgts);
	g->tpwgts = ALLOC_N(float,n_parts);
	get_partwgt(g->tpwgts,v_tpwgts,n_parts);

	switch(flag) {
	case 0:
	    if (n_parts<=8) {
		func = METIS_WPartGraphRecursive;
	    } else {
		func = METIS_WPartGraphKway;
	    }
	    break;
	case 1:
	    func = METIS_WPartGraphRecursive;
	    break;
	case 2:
	    func = METIS_WPartGraphKway;
	    break;
	default:
	    rb_bug("invalid flag");
	}

	func(&n_vertex,
	     g->xadj, g->adjncy, g->vwgt, g->adjwgt,
	     &wgtflag, &numflag,
	     &n_parts, g->tpwgts,
	     options,
	     &edgecut, g->part );

    } else if (rb_obj_is_kind_of(v_tpwgts, rb_cNumeric)) {
	n_parts = NUM2IDXTYPE(v_tpwgts);

	switch(flag) {
	case 0:
	    if (n_parts<=8) {
		func = METIS_PartGraphRecursive;
	    } else {
		func = METIS_PartGraphKway;
	    }
	    break;
	case 1:
	    func = METIS_PartGraphRecursive;
	    break;
	case 2:
	    func = METIS_PartGraphKway;
	    break;
	default:
	    rb_bug("invalid flag");
	}

	func(&n_vertex,
	     g->xadj, g->adjncy, g->vwgt, g->adjwgt,
	     &wgtflag, &numflag,
	     &n_parts,
	     options,
	     &edgecut, g->part );

    } else {
	rb_raise(rb_eArgError,"the last argument must be Array or Numeric");
    }

    // convert array
    v_part = rb_ary_new2(n_vertex);
    for (i=0; i<n_vertex; i++) {
	rb_ary_push( v_part, IDXTYPE2NUM(g->part[i]) );
    }

    if (edgecut==0) {
	rb_warning("edgecut==0");
    }

    return v_part;
}


/*
 *  call-seq:
 *     METIS.part_graph( adj_idx, adjncy, [vertex_wgt|nil], [adj_wgt|nil], [n_part|part_wgt] ) => part
 *
 *  Partition a Graph into <i>n</i> parts to minimize the edgecut.
 *  If the last argument is a Numeric, the graph is partitioned into <i>n</i>-equal-sized parts.
 *  If the last argument is an Array, the graph is partitioned into weighted parts.
 *  If n_part <= 8, it uses the multilevel recursive bisection method.
 *  If n_part >  8, it uses the multilevel k-way partitioning algorithm.
 */

static VALUE
metis_part_graph(VALUE m, VALUE a1, VALUE a2, VALUE a3, VALUE a4, VALUE a5)
{
    return metis_part_graph_main(a1,a2,a3,a4,a5,0);
}


/*
 *  call-seq:
 *     METIS.part_graph_recursive( adj_idx, adjncy, [vertex_wgt|nil], [adj_wgt|nil], [n_part|part_wgt] ) => part
 *
 *  Partition a Graph into <i>n</i> parts to minimize the edgecut.
 *  If the last argument is a Numeric, the graph is partitioned into <i>n</i>-equal-sized parts.
 *  If the last argument is an Array, the graph is partitioned into weighted parts.
 *  It uses the multilevel recursive bisection method.
 */

static VALUE
metis_part_graph_recursive(VALUE m, VALUE a1, VALUE a2, VALUE a3, VALUE a4, VALUE a5)
{
    return metis_part_graph_main(a1,a2,a3,a4,a5,1);
}

/*
 *  call-seq:
 *     METIS.part_graph_kway( adj_idx, adjncy, [vertex_wgt|nil], [adj_wgt|nil], [n_part|part_wgt] ) => part
 *
 *  Partition a Graph into <i>n</i> parts to minimize the edgecut.
 *  If the last argument is a Numeric, the graph is partitioned into <i>n</i>-equal-sized parts.
 *  If the last argument is an Array, the graph is partitioned into weighted parts.
 *  It uses the multilevel k-way partitioning algorithm.
 */

static VALUE
metis_part_graph_kway(VALUE m, VALUE a1, VALUE a2, VALUE a3, VALUE a4, VALUE a5)
{
    return metis_part_graph_main(a1,a2,a3,a4,a5,2);
}



/* ------------------ Multi-Constraint Partitioning ------------------- */

static void
get_mc_vwgt(idxtype *a, VALUE v_vwgt, idxtype n_vertex, idxtype ncon)
{
    idxtype i;
    long    j, k;
    idxtype n_vwgt;
    VALUE   v;

    n_vwgt = n_vertex * ncon;
    i = j = 0;
    while (j < RARRAY_LEN(v_vwgt)) {
	v = RARRAY_PTR(v_vwgt)[j];
	if (TYPE(v)==T_ARRAY) {
	    if (RARRAY_LEN(v) != ncon) {
		rb_raise(rb_eArgError,"ncon(=%"IDXFMT"d) != array.size(=%ld)",
			 ncon,RARRAY_LEN(v));
	    }
	    for (k=0; k<ncon; k++) {
		a[i++] = NUM2IDXTYPE(RARRAY_PTR(v)[k]);
		if (i > n_vwgt) {
		    rb_raise(rb_eArgError,"too many vertex weights");
		}
	    }
	    j++;
	} else {
	    for (k=0; k<ncon; k++) {
		a[i++] = NUM2IDXTYPE(RARRAY_PTR(v_vwgt)[j++]);
		if (i > n_vwgt) {
		    rb_raise(rb_eArgError,"too many vetex weights");
		}
	    }
	}
    }
}


static void
get_ubvec(float *a, VALUE rbary, long n)
{
    long i;

    if (NIL_P(rbary)) {
	for (i=0; i<n; i++) {
	    a[i] = 1.05;
	}
    } else {
	for (i=0; i<n; i++) {
	    a[i] = (float)NUM2DBL(RARRAY_PTR(rbary)[i]);
	}
    }
}


static VALUE
metis_mc_part_graph_main(VALUE v_ncon,
			 VALUE v_xadj,
			 VALUE v_adjncy,
			 VALUE v_vwgt,
			 VALUE v_adjwgt,
			 VALUE v_ubvec,
			 VALUE v_nparts,
			 int flag)
{
    idxtype i;
    idxtype ncon;
    idxtype n_vertex;
    idxtype n_edges2;
    idxtype n_parts;

    idxtype wgtflag = 0;
    idxtype numflag = 0;

    idxtype options[10] = {0,0,0,0,0,0,0,0,0,0};
    idxtype edgecut = 0;

    struct GraphData *g;
    volatile VALUE v;
    volatile VALUE v_part;

    char err_str[10] = "";
    int err_str_len = 0;

    // check argument type
    check_array(v_xadj,   "1");
    check_array(v_adjncy, "2");
    check_array(v_vwgt,   "3");
    check_array2(v_adjwgt,"4");

    if (err_str_len > 0) {
	err_str[err_str_len] = '\0';
	rb_raise(rb_eArgError,"[%s]-th arguments are not an Array",err_str);
    }

    n_vertex = RARRAY_LEN(v_xadj);
    n_edges2 = RARRAY_LEN(v_adjncy);

    ncon = NUM2IDXTYPE(v_ncon);
    if (ncon < 2 || ncon >= 15) {
	rb_raise(rb_eArgError,"ncon(=%"IDXFMT"d) must be 1 < ncon < 15",ncon);
    }

    n_parts = NUM2IDXTYPE(v_nparts);
    if (n_parts < 1) {
	rb_raise(rb_eArgError,"nparts(=%"IDXFMT"d) must be > 0",n_parts);
    }

    if (!NIL_P(v_adjwgt)) {
	if (RARRAY_LEN(v_adjwgt) != n_edges2) {
	    rb_raise(rb_eArgError,"adjncy.length(=%"IDXFMT"d) != adjwgt.length(=%ld)",
		     n_edges2, RARRAY_LEN(v_adjwgt) );
	}
	wgtflag = 1;
    }

    // wrap as a Ruby-object to be GC-ed automatically
    g = ALLOC(struct GraphData);
    MEMZERO(g,struct GraphData,1);
    v = Data_Wrap_Struct(rb_cData,0,g_free,g);

    // allocate array
    get_idxary(g->xadj,v_xadj,n_vertex);
    n_vertex -= 1;
    get_idxary(g->adjncy,v_adjncy,n_edges2);

    // vertex weight for multi-constraint
    g->vwgt = ALLOC_N(idxtype, n_vertex * ncon);
    get_mc_vwgt(g->vwgt, v_vwgt, n_vertex, ncon);

    // edge weight
    if (wgtflag==1) {
	get_idxary(g->adjwgt,v_adjwgt,n_edges2);
    }

    // result array
    g->part = ALLOC_N(idxtype,n_vertex);

    //
    if (flag==0) {
	if (n_parts<=8) {
	    //METIS_PartGraphRecursive;
	    flag = 1;
	    } else {
	    //METIS_PartGraphKway;
	    flag = 2;
	}
    }
    printf("flag=%d\n",flag);

    if (flag==1) {
	METIS_mCPartGraphRecursive
	    ( &n_vertex, &ncon,
	      g->xadj, g->adjncy, g->vwgt, g->adjwgt,
	      &wgtflag, &numflag,
	      &n_parts,
	      options,
	      &edgecut, g->part );
    } else if (flag==2) {
	// ubvec This is a vector of size ncon that specifies the load
	// imbalance tolerances for each one of the ncon constraints.
	// Each tolerance should be greater than 1.0 (preferably greater than 1.03).
	g->ubvec = ALLOC_N(float, ncon);
	get_ubvec(g->ubvec, v_ubvec, ncon);

	//g_debug_print(g,n_vertex,ncon);

	METIS_mCPartGraphKway
	    ( &n_vertex, &ncon,
	      g->xadj, g->adjncy, g->vwgt, g->adjwgt,
	      &wgtflag, &numflag,
	      &n_parts,
	      g->ubvec,
	      options,
	      &edgecut, g->part );
    } else {
	rb_bug("invalid flag");
    }

    // convert array
    v_part = rb_ary_new2(n_vertex);
    for (i=0; i<n_vertex; i++) {
	rb_ary_push( v_part, IDXTYPE2NUM(g->part[i]) );
    }

    if (edgecut==0) {
	rb_warning("edgecut==0");
    }

    return v_part;
}


/*
 *  call-seq:
 *     METIS.mc_part_graph( ncon, adj_idx, adjncy, [vertex_wgt|nil], [adj_wgt|nil], [ubvec|nil], [n_part|part_wgt] ) => part
 *
 *  Partition a Multi-Constraint Graph into <i>n</i> parts to minimize the edgecut.
 *  If the last argument is a Numeric, the graph is partitioned into <i>n</i>-equal-sized parts.
 *  If the last argument is an Array, the graph is partitioned into weighted parts.
 *  If n_part <= 8, it uses the multilevel recursive bisection method.
 *  If n_part >  8, it uses the multilevel k-way partitioning algorithm.
 */

static VALUE
metis_mc_part_graph(VALUE m, VALUE a1, VALUE a2, VALUE a3, VALUE a4, VALUE a5, VALUE a6, VALUE a7)
{
    return metis_mc_part_graph_main(a1,a2,a3,a4,a5,a6,a7,0);
}


/*
 *  call-seq:
 *     METIS.mc_part_graph_recursive( ncon, adj_idx, adjncy, [vertex_wgt|nil], [adj_wgt|nil], [n_part|part_wgt] ) => part
 *
 *  Partition a Multi-Constraint Graph into <i>n</i> parts to minimize the edgecut.
 *  If the last argument is a Numeric, the graph is partitioned into <i>n</i>-equal-sized parts.
 *  If the last argument is an Array, the graph is partitioned into weighted parts.
 *  It uses the multilevel recursive bisection method.
 */

static VALUE
metis_mc_part_graph_recursive(VALUE m, VALUE a1, VALUE a2, VALUE a3, VALUE a4, VALUE a5, VALUE a6)
{
    return metis_mc_part_graph_main(a1,a2,a3,a4,a5,Qnil,a6,1);
}


/*
 *  call-seq:
 *     METIS.mc_part_graph_kway( ncon, adj_idx, adjncy, [vertex_wgt|nil], [adj_wgt|nil], [ubvec|nil], [n_part|part_wgt] ) => part
 *
 *  Partition a Multi-Constraint Graph into <i>n</i> parts to minimize the edgecut.
 *  If the last argument is a Numeric, the graph is partitioned into <i>n</i>-equal-sized parts.
 *  If the last argument is an Array, the graph is partitioned into weighted parts.
 *  It uses the multilevel k-way partitioning algorithm.
 */

static VALUE
metis_mc_part_graph_kway(VALUE m, VALUE a1, VALUE a2, VALUE a3, VALUE a4, VALUE a5, VALUE a6, VALUE a7)
{
    return metis_mc_part_graph_main(a1,a2,a3,a4,a5,a6,a7,2);
}



/* ------------------ METIS_mCPartGraphRecursive2 -------------------
void __cdecl METIS_mCPartGraphRecursive2(
 idxtype *nvtxs, idxtype *ncon, idxtype *xadj, idxtype *adjncy,
 idxtype *vwgt, idxtype *adjwgt, idxtype *wgtflag, idxtype *numflag, idxtype *nparts,
 float *tpwgts, idxtype *options, idxtype *edgecut, idxtype *part);
*/

static VALUE
metis_mc_part_graph_recursive2(VALUE mod,
			       VALUE v_ncon,
			       VALUE v_xadj,
			       VALUE v_adjncy,
			       VALUE v_vwgt,
			       VALUE v_adjwgt,
			       VALUE v_tpwgts)
{
    idxtype i;
    idxtype ncon;
    idxtype n_vertex;
    idxtype n_edges2;
    idxtype n_parts;

    idxtype wgtflag = 0;
    idxtype numflag = 0;

    idxtype options[10] = {0,0,0,0,0,0,0,0,0,0};
    idxtype edgecut = 0;

    struct GraphData *g;
    volatile VALUE v;
    volatile VALUE v_part;

    char err_str[10] = "";
    int err_str_len = 0;

    ncon = NUM2IDXTYPE(v_ncon);
    if (ncon < 2 || ncon >= 15) {
	rb_raise(rb_eArgError,"ncon(=%"IDXFMT"d) must be 1 < ncon < 15",ncon);
    }

    // check argument type
    check_array(v_xadj,   "2");
    check_array(v_adjncy, "3");
    check_array(v_vwgt,   "4");
    check_array2(v_adjwgt,"5");

    if (err_str_len > 0) {
	err_str[err_str_len] = '\0';
	rb_raise(rb_eArgError,"[%s]-th arguments are not an Array",err_str);
    }

    n_vertex = RARRAY_LEN(v_xadj);
    n_edges2 = RARRAY_LEN(v_adjncy);

    if (!NIL_P(v_adjwgt)) {
	if (RARRAY_LEN(v_adjwgt) != n_edges2) {
	    rb_raise(rb_eArgError,"adjncy.length(=%"IDXFMT"d) != adjwgt.length(=%ld)",
		     n_edges2, RARRAY_LEN(v_adjwgt) );
	}
	wgtflag = 1;
    }

    // wrap as a Ruby-object to be GC-ed automatically
    g = ALLOC(struct GraphData);
    MEMZERO(g,struct GraphData,1);
    v = Data_Wrap_Struct(rb_cData,0,g_free,g);

    // allocate array
    get_idxary(g->xadj,v_xadj,n_vertex);
    n_vertex -= 1;
    get_idxary(g->adjncy,v_adjncy,n_edges2);

    // vertex weight for multi-constraint
    g->vwgt = ALLOC_N(idxtype, n_vertex * ncon);
    get_mc_vwgt(g->vwgt, v_vwgt, n_vertex, ncon);

    // edge weight
    if (wgtflag==1) {
	get_idxary(g->adjwgt,v_adjwgt,n_edges2);
    }

    // result array
    g->part = ALLOC_N(idxtype,n_vertex);

    // partition weight
    if (TYPE(v_tpwgts) == T_ARRAY) {
	n_parts = RARRAY_LEN(v_tpwgts);
	g->tpwgts = ALLOC_N(float,n_parts);
	get_partwgt(g->tpwgts,v_tpwgts,n_parts);
    } else if (rb_obj_is_kind_of(v_tpwgts, rb_cNumeric)) {
	n_parts = NUM2IDXTYPE(v_tpwgts);
	g->tpwgts = ALLOC_N(float,n_parts);
	for (i=0; i<n_parts; i++) {
	    g->tpwgts[i] = 1.0/n_parts;
	}
    } else {
	rb_raise(rb_eArgError,"nparts must be Array (tpwgts) or Numeric (nparts)");
    }

    //
    METIS_mCPartGraphRecursive2( &n_vertex, &ncon,
				 g->xadj, g->adjncy, g->vwgt, g->adjwgt,
				 &wgtflag, &numflag,
				 &n_parts, g->tpwgts,
				 options,
				 &edgecut, g->part );

    // convert array
    v_part = rb_ary_new2(n_vertex);
    for (i=0; i<n_vertex; i++) {
	rb_ary_push( v_part, IDXTYPE2NUM(g->part[i]) );
    }

    if (edgecut==0) {
	rb_warning("edgecut==0");
    }

    return v_part;
}


/* ------------------ Initialize ------------------- */

void
Init_metis()
{
    VALUE rb_mMetis;

    rb_mMetis = rb_define_module("Metis");
    rb_define_module_function(rb_mMetis, "part_graph", metis_part_graph, 5);
    rb_define_module_function(rb_mMetis, "part_graph_recursive", metis_part_graph_recursive, 5);
    rb_define_module_function(rb_mMetis, "part_graph_kway", metis_part_graph_kway, 5);

    rb_define_module_function(rb_mMetis, "mc_part_graph", metis_mc_part_graph, 7);
    rb_define_module_function(rb_mMetis, "mc_part_graph_recursive", metis_mc_part_graph_recursive, 6);
    rb_define_module_function(rb_mMetis, "mc_part_graph_kway", metis_mc_part_graph_kway, 7);

    rb_define_module_function(rb_mMetis, "mc_part_graph_recursive2", metis_mc_part_graph_recursive2, 6);
}
