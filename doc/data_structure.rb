#!/usr/bin/env ruby

class DataStruct
  EMPTY_STRING = ''.freeze

  attr_accessor :name, :slots, :memory
  attr_accessor :color, :data
  attr_accessor :links

  def initialize
    @slots = [ ]
    @slot_by_name = { }
    @links = [ ]
  end

  def data!
    @data ||=
      DataStruct::Memory.new
  end

  def color= c
    c = '#C2B280' if c == :ecru
    @color = c
  end

  def struct_path
    "#{name}"
  end

  def dot_id
    name.to_s.inspect
  end

  def dot_node_color
    @dot_node_color ||=
      case color
      when nil
        ""
      when :white
        "style=dotted"
      else
        "color=#{color.to_s.inspect}"
      end
  end

  def dot_node
    if data
      data.dot_subgraph_begin
    end

    puts <<"END"
#{dot_id} [
   label = #{dot_label}
   shape = "record"
   #{dot_node_color}
   #{data && "rank=-1"}
]
END

    if data
      data.dot_nodes
      data.dot_subgraph_end
    end
  end

  def dot_edge
    index = -1
    slots.each do | s |
      s.dot_edge
    end
    if data
      data.objects.each do | o |
        # Used only for grouping
        puts "#{dot_id} -> #{o.dot_id} [ style=\"invis\" ];"
      end
      data.dot_edges
    end
    if data && links
      links.each do | o |
        # This generates warnings, but produces better results.
        # dot -Tpng:cairo:cairo -o "doc/data_structure.png" "doc/data_structure.dot"
        # Warning: The use of "subgraph clusterM1", line 142, without a body is deprecated.
        # This may cause unexpected behavior or crash the program.
        # ...
        puts "subgraph #{data.dot_id} -> subgraph #{o.data.dot_id} [ style=invis ];"
        # puts "#{data.dot_id} -> #{o.data.dot_id} [ style=invis ];"
      end
    end
  end

  def dot_label 
    a = [
         "<sn>[#{name}]",
        ]
    slots.map do | s |
      a << s.dot_label + "\\l"
    end
    a = a.join("|")

    a = "\"#{a}\""
  end

  def << slot
    case slot
    when String, Symbol
      s = Slot.new
      s.name = slot.to_sym
      slot = s
    when Array
      s = Slot.new
      s.name, s.value = *slot
      slot = s
    when Hash
      s = Slot.new
      s.name, s.value = slot[:name], slot[:value]
      slot = s
    end

    slot.struct = self
    slot.index = @slots.size
    @slots << slot
    @slot_by_name[slot.name] = slot
  end

  def method_missing sel, *args
    case
    when s = @slot_by_name[sel]
      s.value
    when (sel = sel.to_s) && sel.sub!(/=$/, '') && (s = @slot_by_name[sel.to_sym]) && args.size == 1
      s.value = args.first
    else
      super
    end
  end

  def [] slot
    @slot_by_name[slot].value
  end

  def []= slot, value
    @slot_by_name[slot].value = value
  end

  def init! slot
    n = :"#{slot}_next"
    p = :"#{slot}_prev"
    unless self[n]
      self[n] = self[p] = self
    end
  end

  def insert! slot, other
    init! slot

    n = :"#{slot}_next"
    p = :"#{slot}_prev"

    other[n] = self[n]
    other[p] = self

    self[n][p] = other
    self[n] = other

    other
  end

  def append! slot, other
    init! slot

    n = :"#{slot}_next"
    p = :"#{slot}_prev"

    self[p].insert! slot, other
  end

  class Slot
    attr_accessor :struct, :name, :value, :index

    def struct_path
      "#{struct.path}.#{name}"
    end

    def dot_id
      struct.dot_id + ":\"s#{index}\""
    end

    def dot_label
      "<s#{index}> #{name}: #{dot_label_value}"
    end
    
    def dot_label_value
      case value
      when nil
        EMPTY_STRING
      when DataStruct, DataStruct::Slot
        "&#{value.struct_path}"
      else
        value
      end
    end

    def dot_edge_value
      case value
      when DataStruct
        value.dot_id + ":\"sn\""
      when DataStruct::Slot
        value.dot_id
      else
        nil
      end
    end

    def dot_edge_color
      c = nil
      v = case value
      when DataStruct
        value.dot_node_color
      when DataStruct::Slot
        c = value.color
        nil
      else
        nil
      end
      if c 
        "color=#{c.to_s.inspect}"
      else
        v
      end
    end


    def dot_edge
      v = dot_edge_value
      if v
        puts <<"END"
#{dot_id} -> #{v} [ #{dot_edge_color} ];
END
      end
    end
  end

  class Memory
    attr_accessor :objects

    def initialize
      @objects = [ ]
    end

    @@dot_id = 0
    def dot_id
      @dot_id ||= 
        "clusterM#{@@dot_id += 1}".inspect.freeze
    end

    @@file_id = 0
    def dot file = nil
      stdout = $stdout
      file ||= "dot#{@@file_id += 1}.dot"
      File.open(file, "w+") do | fh |
        $stdout = fh
        dot_header
        dot_elements
        dot_footer
      end
      if dotty = ENV['dotty']
        Process.fork do 
          system("#{dotty} #{file}")
        end
      end
      file
    ensure 
      $stdout = stdout
    end

    def dot_subgraph
      dot_subgraph_begin
      dot_nodes
      dot_subgraph_end
    end

    def dot_subgraph_begin
      puts <<"END"
subgraph #{dot_id} {
  rankdir = "LR"

END
    end

    def dot_subgraph_end
      puts <<"END"
}
END
    end

    def dot_elements
      dot_nodes
      dot_edges
    end

    def dot_nodes
      @objects.each do | o |
        o.dot_node
      end
    end
    
    def dot_edges
      @objects.each do | o |
        o.dot_edge
      end
    end

    def dot_header
      puts <<"END"
digraph g {
graph [
rankdir = "LR"
clusterrank = "local"
];
node [
fontsize = "6"
shape = "ellipse"
];
edge [
];
END
    end

    def dot_footer
puts <<"END"
}
END
    end

    def struct o
      case o
      when Array
        x = DataStruct.new
        x.name = o.shift
        o.each do | slot |
          x << slot
        end
        o = x
      end
      self << o
    end

    def << o
      o.memory = self
      @objects << o
      o
    end
  end
end



##########################################################################

m = DataStruct::Memory.new
$m = m

tm = m.struct [
               :tm_data,
               :type_next, :type_prev,
               [ :white_count, 0 ],
               [ :ecru_count, 0 ],
               [ :grey_count, 0 ],
               [ :black_count, 0 ],
              ]
tm.color = :green
$tm = tm
# $m.dot


$type_id = 0
def type size
  name = :"tm_type@#{$type_id += 1}"
  t = $m.struct [
                 name, 
                 :type_next, :type_prev, 
                 :block_next, :block_prev, 
                 [ :size, size ],
                 [ :white_count, 0 ],
                 [ :ecru_count, 0 ],
                 [ :grey_count, 0 ],
                 [ :black_count, 0 ],
               ]
  t.color = :red

  [ :white, :ecru, :grey, :black ].each do | n |
    cl = t.data!.
      struct [ :"#{name}.#{n}",
               :node_next, :node_prev,
               [ :color, n ],
             ]
    cl.color = n
    t << [ n, cl ]
  end
  
  $tm.append! :type, t

  t
end


$block_id = 0
def block type
  b = $m.struct [
                 :"tm_block@#{$block_id += 1}", 
                 :block_next, :block_prev, 
                 [ :type, type ], 
                 [ :white_count, 0 ],
                 [ :ecru_count, 0 ],
                 [ :grey_count, 0 ],
                 [ :black_count, 0 ],
               ]
  b.color = :blue
  b.data!
  type.links << b
  type.append! :block, b
  b
end

$node_id = 0
def node type, block, color
  n = block.
    data!.
    struct [
            :"tm_node@#{$node_id += 1}", 
            :node_next, :node_prev, 
            [ :color, color ],
            [ :data, "[#{type[:size]} bytes]" ],
           ]
  n.color = color
  type[color].append! :node, n
  type[:"#{color}_count"] += 1
  block[:"#{color}_count"] += 1
  $tm[:"#{color}_count"] += 1
  n
end



t1 = type(16)
#m.dot

t1b1 = block(t1)
t1b2 = block(t1)
#m.dot

node(t1, t1b1, :white)
node(t1, t1b2, :white)
node(t1, t1b2, :white)
node(t1, t1b1, :ecru)
node(t1, t1b2, :ecru)
node(t1, t1b1, :grey)
node(t1, t1b2, :black)
node(t1, t1b2, :black)

=begin
t2 = type(32)
t2b1 = block(t2)
node(t2, t2b1, :white)
node(t2, t2b1, :ecru)
node(t2, t2b1, :ecru)
=end

m.dot ARGV.first
=begin
m.dot

t3 = type(64)
m.dot
=end
