#pragma once

class TestObj : public TNamed {
public:

  TestObj()                                                   : TNamed()                             { printf("TestObj()\n"); }
  TestObj(Double_t v)                                         : TNamed(), value(v)                   { printf("TestObj(%g)\n",v); }
  TestObj(Double_t v, const char* name, const char* title="") : TNamed(name, title), value(v)        { printf("TestObj(%g,\"%s\")\n",v,name); }
  TestObj(const TestObj& o)                                   : TNamed(o), value(o.value)            { printf("TestObj(TestObj(%g,\"%s\"))\n",value,GetName()); }
  TestObj(const TestObj&& o)                                  : TNamed(std::move(o)), value(o.value) { printf("TestObj(TestObj&&(%g,\"%s\"))\n",value,GetName()); }
  ~TestObj()                                                                                         { printf("~TestObj(%g,\"%s\")\n",value,GetName()); value=-3.0; }
  TestObj& operator=(const TestObj& o) { TNamed::operator=(std::move(o)); value=o.value; printf("TestObj = TestObj&&(%g,\"%s\")\n",value,GetName()); return *this; }
  
  Double_t value=-1.0;
  ClassDefOverride(TestObj,1)
};
