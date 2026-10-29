-- Solo una PRIMARY KEY por tabla.
-- espera: SEM014 @ 3:56
CREATE TABLE t (a INT PRIMARY KEY, b INT, PRIMARY KEY (b));
