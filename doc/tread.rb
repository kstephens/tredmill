!#/usr/bin/env ruby

pname = [ :free,  :bottom, :top,  :scan  ]
color = [ :white, :ecru,   :grey, :black ]

n = 10

nodes = [ ]
(0..n).each do | i |
  nodes << { :id => "n#{i}", :next => nil, :prev => nil, :color => nil }
end
i = 0
(0..n).each do | i |
  nodes[i][:next] = nodes[i + 1]
  nodes[i][:prev] = nodes[i - 1]
end
nodes[i][:next] = nodes[0]


ptr = [ nodes[0], nodes[n * 1 / 4], nodes[n * 2 / 4], nodes[n * 3 / 4] ]

np = ptr[0]
(0..4).each do | c |
  until np == ptr[(c + 1) % 4]
    np[:color] = color[c]
    np = np[:next]
  end
end


ptr[0] = { :id => pname[0], :ptr => ptr[0] }
ptr[1] = { :id => pname[1], :ptr => ptr[1] }
ptr[2] = { :id => pname[2], :ptr => ptr[2] }
ptr[3] = { :id => pname[3], :ptr => ptr[3] }

############################################################

color = { 
  :white => 'fillcolor=white fontcolor=black',
  :ecru  => 'fillcolor=red   fontcolor=black',
  :grey  => 'fillcolor=grey  fontcolor=black',
  :black => 'fillcolor=black fontcolor=white',
}

puts <<"END"
digraph "" {
graph [
#  rankdir = "LR"
];
node [
  fontsize = "8"
  shape = "ellipse"
  style = "filled"
];
edge [
  fontsize = "8"
];
subgraph "nodes" {
END

ptr.each do | p |
  puts <<"END"
#{p[:id].to_s.inspect} [ style="filled" shape="box" ];
END
end

nodes.each do | n |
  puts <<"END"
#{n[:id].inspect} [ #{color[n[:color]]} ];
END
end

nodes.each do | n |
  puts <<"END"
#{n[:id].inspect} -> #{n[:next][:id].inspect} [ label="n" ];
#{n[:id].inspect} -> #{n[:prev][:id].inspect} [ label="p" ];
END
end

ptr.each do | p |
  puts <<"END"
#{p[:id].to_s.inspect} -> #{p[:ptr][:id].inspect};
END
end

puts <<"END"
}
END

puts <<"END"
}
END

