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
  end

  it "part_graph 3:1" do
    Metis.part_graph(@xadj, @adjncy, nil, nil, [0.75,0.25]).should be_wpart_of [0.75,0.25]
  end

  it "part_graph 2:2" do
    Metis.part_graph(@xadj, @adjncy, nil, nil, [0.5,0.5]).should be_wpart_of [0.5,0.5]
  end

  it "part_graph 1:3" do
    Metis.part_graph(@xadj, @adjncy, nil, nil, [0.25,0.75]).should be_wpart_of [0.25,0.75]
  end

  it "part_graph 1:1:2" do
    Metis.part_graph(@xadj, @adjncy, nil, nil, [0.25,0.25,0.5]).should be_wpart_of [0.25,0.25,0.5]
  end
end


describe "Graph[ 0 - 1 - 2 - 3 - 4 ]" do
  before do
    @xadj = [0,1,3,5,7,8]
    @adjncy = [1, 0,2, 1,3, 2,4, 3]
  end

  it "part_graph 1:4" do
    Metis.part_graph(@xadj, @adjncy, nil, nil, [0.2,0.8]).should be_wpart_of [0.2,0.8]
  end

  it "part_graph 2:3" do
    Metis.part_graph(@xadj, @adjncy, nil, nil, [0.4,0.6]).should be_wpart_of [0.4,0.6]
  end

  it "part_graph 3:2" do
    Metis.part_graph(@xadj, @adjncy, nil, nil, [0.6,0.4]).should be_wpart_of [0.6,0.4]
  end

  it "part_graph 4:1" do
    Metis.part_graph(@xadj, @adjncy, nil, nil, [0.8,0.2]).should be_wpart_of [0.8,0.2]
  end
end
