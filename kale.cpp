/*
The characters include [a-zA-Z0-9+-EOF]
*/
#include "llvm/ADT/STLExtras.h"
#include<string>
#include<cctype>    //a set of functions to classify and transform individual characters
#include<algorithm>
#include<cstdio>
#include<cstdlib>
#include<map>
#include<memory>    //manage dynamic memory
#include<vector>

/*
lexer
return enum token for defined, or [0-255] for unknown characters. 
*/

enum token {
    tok_eof = -1,    //for EOF
    //commands
    tok_def = -2,    //for def 
    tok_extern = -3,    //for extern
    //primary
    tok_identifier = -4,
    tok_number = -5,
};

static std::string IdentifierStr;
static double NumVal;

static int getTok(){
    static int LastChar = ' ';

    while (isspace(LastChar))    //skip the spaces
        LastChar = getchar();
    
    if (isalpha(LastChar)){
        IdentifierStr = LastChar;    //the first place of an identifier must be an alphabet
        while(isalnum((LastChar = getchar())))    //check if X is alphanumeric
            IdentifierStr += getchar();
        if(IdentifierStr == "def")
            return tok_def;
        if(IdentifierStr == "extern")
            return tok_extern;
        return tok_identifier;    //why not using else if ? Is def & extern a subset of identifier?
    }
    
    //TODO order
    if(isdigit(LastChar)||LastChar == '.'){    //the order of . and digits?
        std::string NumStr;
        do{
            NumStr += LastChar;
            LastChar = getchar();
        }while(isdigit(LastChar)||LastChar == '.');
        NumVal = strtod(NumStr.c_str(), nullptr);    //strtod:string => double, c_str(): string => point to it, 0 may be the 0 address 1.2.3.4??
        return tok_number;
    }

    if(LastChar == '#'){    //# stands for the beginning of comments, of course it can be replaced by others
        do{
            LastChar = getchar();
        }while(LastChar != EOF && LastChar != '\n' && LastChar != '\r');
        if(LastChar != EOF)
            return getTok();
    }

    if(LastChar ==EOF)
        return tok_eof;
    int Thischar = LastChar;
    LastChar = getchar();
    return Thischar;    // In the final case, the LastChar must be an operator character +, - 

}

/*
parser
*/

class ExprAST {
    public:
        virtual ~ExprAST() {}  //Cons
};

class NumberExprAST : public ExprAST{
    double Val;
    NumberExprAST(double Val) : Val(Val) {}
};

class VariableExprAST : public ExprAST{
    std::string Name;
    VariableExprAST(const std::string &Name) : Name(Name);
};

class CallExprAST : public ExprAST{
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;

    CallExprAST(connst std::string &Callee
                std::vector<std::unique_ptr<ExprAST>> Args)
        : Callee(Callee), Args(std::move(Args)) {}
};

class BinaryExprAST : public ExprAST{
    char Op;
    std::vector<std::unique_ptr<ExprAST>> LExp, RExp;

    BinaryExprAST(char Op,
                 std::vector<std::unique_ptr<ExprAST>> LExp,
                 std::vector<std::unique_ptr<ExprAST>> RExp)
        : Op(Op), LExp(std::move(LExp)), RExp(std::move(RExp)) {}
};
/*
class FPrototypeExprAST : public ExprAST{
    std::string FPrototype;
    std::vector<std::unique_ptr<ExprAST>> Args;

    FPrototypeExprAST(const std::string &FPrototype,
                     std::vector<std::unique_ptr<ExprAST>> Args)
        : FPrototype(FPrototype), Args(std::move(Args)) {}
}*/

class FPrototypeExprAST : public ExprAST{
    std::string Name;
    std::vector<std::string> Args;

    FPrototypeExprAST(const std::string &Name,
                     std::vector<std::string> Args)
        : Name(Name), Args(std::move(Args)) {}
    
    const std::string &getName() const {return Name;}
};

class FunctionDefineExprAST : public ExprAST{
    std::unique_ptr<FPrototypeExprAST> FProto;
    std::unique_ptr<ExprAST> FBody;

    FunctionDefineExprAST(std::unique_ptr<FPrototypeExprAST> FProto,
                        std::unique_ptr<ExprAST> FBody)
        : FProto(std::move(FProto)), FBody(std::move(FBody)) {}

};

//an example of x - y
auto LT = llvm::make_unique<VariableExprAST>("x");
auto RT = llvm::make_unique<VariableExprAST>("y");
auto Result = llvm::make_unique<BinaryExprAST>('-', std::move(LT),
                                               std::move(RT));

static int Curtok;  //a simple token buffer
static int getNextTok(){
    return Curtok = getTok(); 
}

std::unique_ptr<ExprAST> LogError(const char *Str){
    fprintf(stderr, "Log Error: %s\n", Str);
    return nullptr;
}

static std::unique_ptr<NumberExprAST> parseNumberExpr(){
    auto Result = llvm::make_unique<NumberExprAST>(NumVal);    create a NumberExprAST node
    getNextTok();    //go to the next
    return std::move(Result);
}


//  '('expression*')'
static std::unique_ptr<ExprAST> parseParenExpr(){
    getNextTok();
    auto V = parseExpression();
    if(!V){
        return nullptr;
    }

    if(Curtok!=')')
        return LogError("Expect a ')'");
    getNextTok();
    return std::move(V);    //TODO the type of V
}

//  variable and function calls
//  identifier identifier'('expression')'

static std::unique_ptr<ExprAST> parseIdentifierExpr(){
    std::string idName = IdentifierStr;
    getNextTok();
    if(Curtok!='('){
        return llvm::make_unique<VariableExprAST>(idName);
    }

    //function call begins
    getNextTok();
    std::vector<std::unique_ptr<ExprAST>> Args;
    if(Curtok!=')'){    //the very next token is not ')'
        while(1){
            // recursively check parameters
            if(auto Arg = parseExpression())
                Args.push_back(std::move(Arg));
            else
                return nullptr;
            // if ')' occurred, break
            if(Curtok == ')')
                break;
            //if not ')' nor ',', error
            if(Curtok !=',')
                return LogError("Parse Error");
            getNextTok();
        }
    }
    getNextTok();
    return llvm::make_unique<CallExprAST> (idName, std::move(Args));
}

//Primary
//case identifier
//case Number
//case '()'

static std::unique_ptr<ExprAST> ParsePrimary(){
    switch(Curtok){
        case tok_identifier:
            std::unique_ptr<ExprAST> parseIdentifierExpr();
        case tok_number:
            std::unique_ptr<ExprAST> parseNumberExpr();
        case  '(':
            std::unique_ptr<ExprAST> parseParenExpr();
        default:
            return LogError("Not valid ")
    }
}

//binary precedence mapping
std::map<char, int> BinaryPrecedence;

//TODO
BinaryPrecedence['<'] = 10;
BinaryPrecedence['+'] = 20;
BinaryPrecedence['-'] = 20;
BinaryPrecedence['*'] = 40;
BinaryPrecedence['/'] = 40;

static int GetTokPrecedence(){
    if(!isascii(Curtok))
        //return EOF
        return -1;
    int TokPrece = BinaryPrecedence[Curtok];
    //TODO
    if(TokPrece<=0)
        return -1;
    return TokPrece;
}

//parse expression
static std::unique_ptr<ExprAST> parseExpression(){
    auto LT = ParsePrimary();
    if (!LT)
        return nullptr;
    return ParseBinopRT(0, std::move(LT));
}

//e.g.a+b*c
static std::unique_ptr<ExprAST> ParseBinopRT(int ExprPrece, std::unique_ptr<ExprAST> LT){
    while(true){
        int TokPrece = GetTokPrecedence();
        if(TokPrece < ExprPrece)
            return LT;
        //then it is a binary operator
        int Binop = Curtok;    //'eat' the binary, aka '+'
        getNextTok();    //Curtok point to b

        auto RT = ParsePrimary();    //b point to RT, ParsePrimary includes another getNextTok()
        if(!RT)
            return nullptr;
        //below is how to decide the parse order according to the precedence
        int NextTokPrece = GetTokPrecedence();    //the next token *
        if(TokPrece < NextTokPrece){
            RT = ParseBinopRT(TokPrece+1, std::move<ExprAST> RT)    //Q: what if more than 20 +?  A:even 10 is OK I think
            if(!RT)
                return nullptr;
        }
        LT = llvm::make_unique<BinaryExprAST>(Binop, std::move(LT), std::move(RT))
    }
}

//primary(Iden, number, para), binary, and prototype
//extern declaration and funciton prototype
//function prototypes
static std::unique_ptr<FPrototypeExprAST> ParseFPrototype(){
    if(Curtok!=tok_identifier)
        return LogError("Function name expected!");
    std::string FName = IdentifierStr;
    getNextTok();    //of course, IdentifierStr updated!

    if(Curtok!='(')
        return LogError("a ( expected for a function parameters list";)
    std::vector<std::string> ArgNames;
    while(getNextTok()==tok_identifier){
        ArgNames.push_back(std::move(IdentifierStr));
    }
    if(Curtok!=')')
        return LogError("a ) expected");
    getNextTok();

    return llvm::make_unique<FPrototypeExprAST>(FName, std::move(ArgNames));
}

//def XXX
static std::unique_ptr<FunctionDefineExprAST> ParseFunctionDefine(){
    getNextTok();    //eat def
    auto FProto = ParseFPrototype();
    if(!FProto)
        return nullptr;
    
}