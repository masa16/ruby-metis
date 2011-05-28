require "rspec"
require File.dirname(File.expand_path(__FILE__))+"/../ext/metis"

module RSpec
  module Matchers
    # :call-seq:
    #   should be_instance_of(expected)
    #   should_not be_instance_of(expected)
    #
    # Passes if actual.instance_of?(expected)
    #
    # == Examples
    #
    #   5.should be_instance_of(Fixnum)
    #   5.should_not be_instance_of(Numeric)
    #   5.should_not be_instance_of(Float)

    def be_wpart_of(expected)
      Matcher.new :be_wpart_of, expected do |_expected_|
        match do |actual|
          npart = _expected_.size
          hist = [0]*npart
          actual.each do |i|
            hist[i] += 1
          end
          #p ["hist:",hist]
          nvertex = actual.size
          (0...npart).all? do |i|
            x = _expected_[i] * nvertex
            (x.round - hist[i]) <= 0.5
          end
        end
      end
    end

  end
end


describe "Graph[ 0 - 1 - 2 - 3 ]" do
  before do
    @xadj = [0,1,3,5,6]
    @adjncy = [1, 0,2, 1,3, 2]
    #@vwgt = [ [1,0], [1,0], [0,1], [0,1] ]
    @vwgt = [ [1,0], [0,1], [1,0], [0,1] ]
  end

  it "mc_part_graph vwgt=[ [1,0], [1,0], [0,1], [0,1] ]" do
    vwgt = [ [1,0], [1,0], [0,1], [0,1] ]
    #p Metis.mc_part_graph(2, @xadj, @adjncy, @vwgt, nil, nil, 2)
    Metis.mc_part_graph(2, @xadj, @adjncy, @vwgt, nil, nil, 2).should be_wpart_of [0.5,0.5]
  end

  it "mc_part_graph vwgt=[ [1,0], [0,1], [1,0], [0,1] ]" do
    vwgt = [ [1,0], [0,1], [1,0], [0,1] ]
    #p Metis.mc_part_graph(2, @xadj, @adjncy, @vwgt, nil, nil, 2)
    Metis.mc_part_graph(2, @xadj, @adjncy, @vwgt, nil, nil, 2).should be_wpart_of [0.5,0.5]
  end

  it "mc_part_graph vwgt=[ 1,0, 0,1, 1,0, 0,1 ]" do
    vwgt = [ 1,0, 0,1, 1,0, 0,1 ]
    #p Metis.mc_part_graph(2, @xadj, @adjncy, @vwgt, nil, nil, 2)
    Metis.mc_part_graph(2, @xadj, @adjncy, @vwgt, nil, nil, 2).should be_wpart_of [0.5,0.5]
  end

end
