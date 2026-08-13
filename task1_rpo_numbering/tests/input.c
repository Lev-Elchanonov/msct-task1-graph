//=============================================================================
// FILE:
//      input.c
//
// DESCRIPTION:
//      Sample input file
//
// License: MIT
//=============================================================================
int
foo(int a) {
  return a * 2;
}

int
bar(int a, int b) {
  return (a + foo(b) * 2);
}

int
fez(int a, int b, int c) {
  return (a + bar(a, b) * 2 + c * 3);
}

int
main(int argc, char *argv[]) {
  int a = 123;
  int ret = 0;

  if (a > 100)
    ret += bar(a, ret);
  else
    ret += fez(a, ret, 123);

  return ret;
}
