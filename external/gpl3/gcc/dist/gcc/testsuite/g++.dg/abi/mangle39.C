// PR c++/42338
// { dg-options "-std=c++0x" }
// { dg-final { scan-assembler "_Z1fIPiEDTcmppfp_Li0EET_" } }
// { dg-final { scan-assembler "_Z1gIiEvRK1AIT_EDTixfp_Li0EE" } }

template<typename T>
auto f(T t) -> decltype(++t, 0)
{
  ++t;
  return 0;
}

template <class T>
struct A
{
  T operator[](int) const { return 0; }
};

template< typename T >
void g(const A<T> &a, decltype(a[0]) t) { }

int main()
{
  f((int*)0);

  A<int> a;
  g(a,1);
}
