def test_case
{"RawParseTree"=>
  [:block,
   [:lasgn, :t, [:lit, 0]],
   [:iter,
    [:call, [:vcall, :ns], :each],
    [:lasgn, :n],
    [:lasgn, :t, [:call, [:lvar, :t], :+, [:array, [:lvar, :n]]]]]],
 "Ruby"=>"t = 0\nns.each { |n| t += n }\n",
 "RubyParser"=>
  s(:block,
   s(:lasgn, :t, s(:lit, 0)),
   s(:iter,
    s(:call, s(:call, nil, :ns, s(:arglist)), :each, s(:arglist)),
    s(:lasgn, :n),
    s(:lasgn, :t, s(:call, s(:lvar, :t), :+, s(:arglist, s(:lvar, :n)))))),
 "Ruby2Ruby"=>"t = 0\nns.each { |n| t = (t + n) }\n"}
end
