function par(n:integer):boolean{if(n==0){return true;}else{return impar(n-1);}} function impar(n:integer):boolean{if(n==0){return false;}else{return par(n-1);}} print(par(4));
