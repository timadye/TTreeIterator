#pragma once

class TestObj : public TNamed {
public:

  TestObj()                                                   : TNamed()                             { if (verbose>=1) printf("TestObj()@%p\n",this); }
  TestObj(Double_t v)                                         : TNamed(), value(v)                   { if (verbose>=1) printf("TestObj(%g)@%p\n",v,this); }
  TestObj(Double_t v, const char* name, const char* title="") : TNamed(name, title), value(v)        { if (verbose>=1) printf("TestObj(%g,\"%s\")@%p\n",v,name,this); }
  TestObj(const TestObj& o)                                   : TNamed(o), value(o.value)            { if (verbose>=1) printf("TestObj(TestObj(%g,\"%s\"))@%p\n",value,GetName(),this); }
  TestObj(const TestObj&& o)                                  : TNamed(std::move(o)), value(o.value) { if (verbose>=1) printf("TestObj(TestObj&&(%g,\"%s\"))@%p\n",value,GetName(),this); }
  ~TestObj()                                                                                         { if (verbose>=1) printf("~TestObj(%g,\"%s\")@%p\n",value,GetName(),this); value=-3.0; }
  TestObj& operator=(const TestObj& o) { TNamed::operator=(std::move(o)); value=o.value; if (verbose>=1) printf("TestObj = TestObj(%g,\"%s\")@%p\n", value,GetName(),this); return *this; }
  TestObj& operator=(      TestObj&&o) { TNamed::operator=(std::move(o)); value=o.value; if (verbose>=1) printf("TestObj = TestObj&&(%g,\"%s\")@%p\n",value,GetName(),this); return *this; }
  
  Double_t value=-1.0;
  static int verbose;
  ClassDefOverride(TestObj,1)
};

int TestObj::verbose = 0;
