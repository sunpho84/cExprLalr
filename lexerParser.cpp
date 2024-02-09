#include <cstdio>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

/// Keep track of what matched
struct Matching
{
  /// Reference to the string view holding the data
  std::string_view& ref;
  
  /// Construct from a string view
  constexpr Matching(std::string_view& in) :
    ref(in)
  {
  }
  
  /// Match any char
  constexpr char matchAnyChar()
  {
    if(not ref.empty())
      {
	const char c=
	  ref.front();
	
	ref.remove_prefix(1);
	
	return c;
      }
    else
      return '\0';
  }
  
  /// Match a single char
  constexpr bool matchChar(const char& c)
  {
    const bool accepting=
      (not ref.empty()) and ref.starts_with(c);
    
    if(accepting)
      ref.remove_prefix(1);
	
    return accepting;
  }
  
  /// Match a char if not in the filt list
  constexpr char matchCharNotIn(const std::string_view& filt)
  {
    if(not ref.empty())
      if(const char c=ref.front();filt.find_first_of(c)==filt.npos)
	{
	  ref.remove_prefix(1);
	  
	  return c;
	}
    
    return '\0';
  }
  
  /// Match a char if it is in the list
  constexpr char matchAnyCharIn(const std::string_view& filt)
  {
    if(not ref.empty())
      {
	const size_t pos=
	  filt.find_first_of(ref.front());
	
	if(pos!=filt.npos)
	  {
	    const char c=
	      ref[pos];
	    
	    ref.remove_prefix(1);
	    
	    return c;
	  }
      }
    
    return '\0';
  }
  
  /// Forbids taking a copy
  Matching(const Matching&)=delete;
  
  /// Forbids default construct
  Matching()=delete;
};

/// Holds a node in the parse tree for regex
struct RegexParserNode
{
  /// Possible ypes of the node
  enum Type{OR,AND,OPT,MANY,NONZERO,CHAR};
  
  /// Specifications of the node type
  struct TypeSpecs
  {
    /// Tag associated to the type
    const char* const tag;
    
    /// Number of subnodes
    const size_t nSubNodes;
    
    /// Symbol representing the node
    const char symbol;
  };
  
  /// Type of present node
  Type type;
  
  /// Collection of the type specifications in the same order of the enum
  static constexpr TypeSpecs typeSpecs[]={
    {"OR",2,'|'},
    {"AND",2,'&'},
    {"OPT",1,'?'},
    {"MANY",1,'?'},
    {"NONZERO",1,'+'},
    {"CHAR",0,'#'}};
  
  /// Subnodes of the present node
  std::vector<RegexParserNode> subNodes;
  
  /// First char matched by the node
  char begChar;
  
  /// Past last char matched by the node
  char endChar;
  
  /// Print the current node, and all subnodes iteratively, indenting more and more
  constexpr void printf(const int& indLv=0) const
  {
    /// String used to indent
    char* ind=new char[indLv+1];
    
    for(int i=0;i<indLv;i++)
      ind[i]=' ';
    
    ind[indLv]='\0';
    ::printf("%s %s ",ind,typeSpecs[type].tag);
    if(type==CHAR)
      ::printf("%c %c",begChar,endChar);
    ::printf("\n");
    
    for(const auto& subNode : subNodes)
      subNode.printf(indLv+1);
    
    delete[] ind;
  }
  
  /// Forbids default construct of the node
  RegexParserNode()=delete;
  
  /// Construct from type, subnodes, beging and past end char
  constexpr RegexParserNode(const Type& type,
			    std::vector<RegexParserNode>&& subNodes,
			    const int begChar=0,
			    const int endChar=0) :
    type(type),
    subNodes(subNodes),
    begChar(begChar),
    endChar(endChar)
  {
  }
};

/////////////////////////////////////////////////////////////////

/// Match two expressions joined by "|"
///
/// Forward declaration
constexpr std::optional<RegexParserNode> matchAndAddPossiblyOrredExpr(Matching& matchIn);

/// Matches a subexpression
constexpr std::optional<RegexParserNode> matchSubExpr(Matching& matchIn)
{
  /// Keep track of the original point in case of needed backup
  std::string_view backup=
    matchIn.ref;
  
  if(matchIn.matchChar('('))
    if(std::optional<RegexParserNode> s=matchAndAddPossiblyOrredExpr(matchIn);s and matchIn.matchChar(')'))
      return s;

  matchIn.ref=backup;
  
  return {};
}

/// Matches any char
constexpr std::optional<RegexParserNode> matchDot(Matching& matchIn)
{
  if(matchIn.matchChar('.'))
    return RegexParserNode{RegexParserNode::Type::CHAR,{},0,std::numeric_limits<char>::max()};
  
  return {};
}

/// Return the escaped counterpart of the escaped part in a few cases, or the char itself
constexpr char maybeEscape(const char& c)
{
  switch (c)
    {
    case 'b':return '\b';
    case 'n':return '\n';
    case 'f':return '\f';
    case 'r':return '\r';
    case 't':return '\t';
    }
  
  return c;
}

/// Match a char including possible escape
constexpr std::optional<RegexParserNode> matchPossiblyEscapedChar(Matching& matchIn)
{
  if(const char c=matchIn.matchCharNotIn("|*+?()"))
    if(const char d=(c=='\\')?maybeEscape(matchIn.matchAnyChar()):c)
      return RegexParserNode{RegexParserNode::Type::CHAR,{},d,d+1};
  
  return {};
}

/// Match an expression and possible postfix
constexpr std::optional<RegexParserNode> matchAndAddExprWithPossiblePostfix(Matching& matchIn)
{
  using enum RegexParserNode::Type;
  
  /// Result to be returned
  std::optional<RegexParserNode> m;
  
  if(not (m=matchSubExpr(matchIn)))
    if(not (m=matchDot(matchIn)))
      m=matchPossiblyEscapedChar(matchIn);
  
  if(m)
    if(const int c=matchIn.matchAnyCharIn("+?*"))
      m=RegexParserNode{(c=='+')?NONZERO:((c=='?')?OPT:MANY),{std::move(*m)}};
  
  return m;
}

/// Match one or two expressions
constexpr std::optional<RegexParserNode> matchAndAddPossiblyAndedExpr(Matching& matchIn)
{
  /// First and possible only subexpression
  std::optional<RegexParserNode> lhs=
    matchAndAddExprWithPossiblePostfix(matchIn);
  
  if(lhs)
    if(std::optional<RegexParserNode> rhs=matchAndAddPossiblyAndedExpr(matchIn))
      return RegexParserNode{RegexParserNode::Type::AND,{std::move(*lhs),std::move(*rhs)}};
  
  return lhs;
}

/// Match one or two expression, the second is optionally matched
constexpr std::optional<RegexParserNode> matchAndAddPossiblyOrredExpr(Matching& matchIn)
{
  /// First and possible only subexpression
  std::optional<RegexParserNode> lhs=
    matchAndAddPossiblyAndedExpr(matchIn);
  
  /// Keep track of the original point in case of needed backup
  std::string_view backup=matchIn.ref;
  
  if(matchIn.matchChar('|'))
    if(std::optional<RegexParserNode> rhs=matchAndAddPossiblyAndedExpr(matchIn))
      return RegexParserNode{RegexParserNode::Type::OR,{std::move(*lhs),std::move(*rhs)}};
  
  matchIn.ref=backup;
  
  return lhs;
}

constexpr bool test(const char* str)
{
  std::string_view toParse=str;
  
  Matching match(toParse);
  
  std::optional<RegexParserNode> t=
    matchAndAddPossiblyOrredExpr(match);
  
  //if(t and (probe==str+len or (len==0 and *probe=='\0')))
  if(not std::is_constant_evaluated())
    t->printf();
  
  return true;
}

int main(int narg,char** arg)
{
  if(narg>1)
    test(arg[1]);
  else
    const auto i=test("c|d(f?|g)");
  
  return 0;
}
