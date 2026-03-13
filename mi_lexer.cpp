/*
 * Analizador Lexico generado desde YALex
 * Compilar: g++ -std=c++17 -o mi_lexer mi_lexer.cpp
 * Uso: ./mi_lexer <archivo_entrada>
 */

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

// === HEADER ===
// Header: includes necesarios
// === FIN HEADER ===

const char* TOKEN_NAMES[] = {
    "lexbuf",
    "EOL",
    "int(lxm",
    "PLUS",
    "MINUS",
    "TIMES",
    "DIV",
    "LPAREN",
    "RPAREN"
};

const char* TOKEN_ACTIONS[] = {
    "return lexbuf",
    "return EOL",
    "return int(lxm)",
    "return PLUS",
    "return MINUS",
    "return TIMES",
    "return DIV",
    "return LPAREN",
    "return RPAREN"
};

const int NUM_ESTADOS = 10;
const int NUM_SIMBOLOS = 19;
const int ESTADO_INICIAL = 9;

int char_to_sym[256];
void init_char_to_sym() {
    for(int i=0;i<256;i++)char_to_sym[i]=-1;
    char_to_sym[9]=0;
    char_to_sym[10]=1;
    char_to_sym[32]=2;
    char_to_sym[40]=3;
    char_to_sym[41]=4;
    char_to_sym[42]=5;
    char_to_sym[43]=6;
    char_to_sym[45]=7;
    char_to_sym[47]=8;
    char_to_sym[48]=9;
    char_to_sym[49]=10;
    char_to_sym[50]=11;
    char_to_sym[51]=12;
    char_to_sym[52]=13;
    char_to_sym[53]=14;
    char_to_sym[54]=15;
    char_to_sym[55]=16;
    char_to_sym[56]=17;
    char_to_sym[57]=18;
}

int transicion[10][19];
int token_aceptado[10];

void init_tablas() {
    for(int i=0;i<NUM_ESTADOS;i++)for(int j=0;j<NUM_SIMBOLOS;j++)transicion[i][j]=-1;
    transicion[8][9]=8;
    transicion[8][10]=8;
    transicion[8][11]=8;
    transicion[8][12]=8;
    transicion[8][13]=8;
    transicion[8][14]=8;
    transicion[8][15]=8;
    transicion[8][16]=8;
    transicion[8][17]=8;
    transicion[8][18]=8;
    transicion[9][0]=0;
    transicion[9][1]=1;
    transicion[9][2]=0;
    transicion[9][3]=6;
    transicion[9][4]=7;
    transicion[9][5]=4;
    transicion[9][6]=2;
    transicion[9][7]=3;
    transicion[9][8]=5;
    transicion[9][9]=8;
    transicion[9][10]=8;
    transicion[9][11]=8;
    transicion[9][12]=8;
    transicion[9][13]=8;
    transicion[9][14]=8;
    transicion[9][15]=8;
    transicion[9][16]=8;
    transicion[9][17]=8;
    transicion[9][18]=8;
    for(int i=0;i<NUM_ESTADOS;i++)token_aceptado[i]=-1;
    token_aceptado[0]=0;
    token_aceptado[1]=1;
    token_aceptado[2]=3;
    token_aceptado[3]=4;
    token_aceptado[4]=5;
    token_aceptado[5]=6;
    token_aceptado[6]=7;
    token_aceptado[7]=8;
    token_aceptado[8]=2;
}

void analizar(const std::string& entrada) {
    init_char_to_sym(); init_tablas();
    size_t pos=0; int linea=1,columna=1;
    while(pos<entrada.size()) {
        int ea=ESTADO_INICIAL; size_t uap=pos; int ut=-1; size_t i=pos;
        while(i<entrada.size()){int sy=char_to_sym[(unsigned char)entrada[i]];if(sy<0)break;int sg=transicion[ea][sy];if(sg<0)break;ea=sg;i++;if(token_aceptado[ea]>=0){uap=i;ut=token_aceptado[ea];}}
        if(ut>=0&&uap>pos){
            std::string lex=entrada.substr(pos,uap-pos);
            std::string ac=TOKEN_ACTIONS[ut];
            if(std::string(ac).find("return lexbuf")==std::string::npos&&std::string(ac).find("return EOL")==std::string::npos){
                std::string lp;for(char c:lex){if(c=='\n')lp+="\\n";else if(c=='\t')lp+="\\t";else lp+=c;}
                std::cout<<"Token: "<<TOKEN_NAMES[ut]<<"  Lexema: '"<<lp<<"'  Linea: "<<linea<<"  Col: "<<columna<<"\n";
            }
            for(size_t k=pos;k<uap;k++){if(entrada[k]=='\n'){linea++;columna=1;}else columna++;}
            pos=uap;
        } else {
            std::cerr<<"ERROR LEXICO: '"<<entrada[pos]<<"' ("<<(int)(unsigned char)entrada[pos]<<") linea "<<linea<<" col "<<columna<<"\n";
            if(entrada[pos]=='\n'){linea++;columna=1;}else columna++; pos++;
        }
    }
    std::cout<<"\nAnalisis lexico completado.\n";
}

int main(int argc,char*argv[]){
    if(argc!=2){std::cerr<<"Uso: "<<argv[0]<<" <archivo>\n";return 1;}
    std::ifstream f(argv[1]);if(!f.is_open()){std::cerr<<"Error: "<<argv[1]<<"\n";return 1;}
    std::ostringstream ss;ss<<f.rdbuf();std::string e=ss.str();
    std::cout<<"=== ANALIZADOR LEXICO ===\n";analizar(e);return 0;
}
