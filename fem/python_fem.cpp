#ifdef NGS_PYTHON
#include "../ngstd/python_ngstd.hpp"
#include "../ngstd/bspline.hpp"
#include <fem.hpp>
#include <mutex>
using namespace ngfem;
using ngfem::ELEMENT_TYPE;

#include "pml.hpp"

namespace ngfem
{
  extern SymbolTable<double> * constant_table_for_FEM;
  SymbolTable<double> pmlpar;
}


struct PythonCoefficientFunction : public CoefficientFunction {
  PythonCoefficientFunction() : CoefficientFunction(1,false) { ; }

    virtual double EvaluateXYZ (double x, double y, double z) const = 0;

    bp::list GetCoordinates(const BaseMappedIntegrationPoint &bip ) {
        double x[3]{0};
        int dim = bip.GetTransformation().SpaceDim();
        const DimMappedIntegrationPoint<1,double> *ip1;
        const DimMappedIntegrationPoint<2,double> *ip2;
        const DimMappedIntegrationPoint<3,double> *ip3;
        switch(dim) {

            case 1:
                ip1 = static_cast<const DimMappedIntegrationPoint<1,double>*>(&bip);
                x[0] = ip1->GetPoint()[0];
                break;
            case 2:
                ip2 = static_cast<const DimMappedIntegrationPoint<2,double>*>(&bip);
                x[0] = ip2->GetPoint()[0];
                x[1] = ip2->GetPoint()[1];
                break;
            case 3:
                ip3 = static_cast<const DimMappedIntegrationPoint<3,double>*>(&bip);
                x[0] = ip3->GetPoint()[0];
                x[1] = ip3->GetPoint()[1];
                x[2] = ip3->GetPoint()[2];
                break;
            default:
                break;
        }
        bp::list list;
        int i;
        for(i=0; i<dim; i++)
            list.append(x[i]);
        for(i=0; i<3; i++)
            list.append(0.0);
        return list;
    }
};

class PythonCFWrap : public PythonCoefficientFunction , public bp::wrapper<PythonCoefficientFunction> {
    static std::mutex m;
    public:
        PythonCFWrap () : PythonCoefficientFunction() { ; }
        double EvaluateXYZ (double x, double y, double z) const {
            return this->get_override("EvaluateXYZ")(x,y,z);
        }

        double Evaluate (const BaseMappedIntegrationPoint & bip) const {
            double ret = 0;
            m.lock();
            try { 
                ret = this->get_override("Evaluate")(boost::ref(bip)); 
            }
            catch (bp::error_already_set const &) {
                PyErr_Print();
            }
            catch(...) {
                cout << "caught Exception in PythonCoefficientFunction::Evaluate" << endl;
            }
            m.unlock();
            return ret;
        }
};

std::mutex PythonCFWrap::m;

MSVC2015_UPDATE3_GET_PTR_FIX(PythonCFWrap)





/*
shared_ptr<CoefficientFunction> MakeCoefficient (bp::object py_coef)
{
  if (bp::extract<shared_ptr<CoefficientFunction>>(py_coef).check())
    return bp::extract<shared_ptr<CoefficientFunction>>(py_coef)();
  else if (bp::extract<double>(py_coef).check())
    return make_shared<ConstantCoefficientFunction> 
      (bp::extract<double>(py_coef)());
  else
    {
      bp::exec("raise KeyError()\n");
      return nullptr;
    }
}
*/
typedef CoefficientFunction CF;
typedef PyWrapper<CoefficientFunction> PyCF;

PyCF MakeCoefficient (bp::object val)
{
  bp::extract<PyCF> ecf(val);
  if (ecf.check()) return ecf();

  bp::extract<double> ed(val);
  if (ed.check()) 
    return PyCF(make_shared<ConstantCoefficientFunction> (ed()));

  bp::extract<Complex> ec(val);
  if (ec.check()) 
    return PyCF(make_shared<ConstantCoefficientFunctionC> (ec()));

  bp::extract<bp::list> el(val);
  if (el.check())
    {
      Array<shared_ptr<CoefficientFunction>> cflist(bp::len(el()));
      for (int i : Range(cflist))
        cflist[i] = MakeCoefficient(el()[i]).Get();
      return PyCF(MakeDomainWiseCoefficientFunction(move(cflist)));
    }

  bp::extract<bp::tuple> et(val);
  if (et.check())
    {
      Array<shared_ptr<CoefficientFunction>> cflist(bp::len(et()));
      for (int i : Range(cflist))
        cflist[i] = MakeCoefficient(et()[i]).Get();
      return PyCF(MakeVectorialCoefficientFunction(move(cflist)));
    }


  throw Exception ("cannot make coefficient");
}

Array<shared_ptr<CoefficientFunction>> MakeCoefficients (bp::object py_coef)
{
  Array<shared_ptr<CoefficientFunction>> tmp;
  if (bp::extract<bp::list>(py_coef).check())
    {
      auto l = bp::extract<bp::list>(py_coef)();
      for (int i = 0; i < bp::len(l); i++)
        tmp += MakeCoefficient(l[i]).Get();
    }
  else if (bp::extract<bp::tuple>(py_coef).check())
    {
      auto l = bp::extract<bp::tuple>(py_coef)();
      for (int i = 0; i < bp::len(l); i++)
        tmp += MakeCoefficient(l[i]).Get();
    }
  else
    tmp += MakeCoefficient(py_coef).Get();

  // return move(tmp);  // clang recommends not to move it ...
  return tmp;
}



template <typename FUNC>
void ExportStdMathFunction(string name)
{
  bp::def (name.c_str(), FunctionPointer 
           ([] (bp::object x) -> bp::object
            {
              FUNC func;
              if (bp::extract<PyCF>(x).check())
                {
                  auto coef = bp::extract<PyCF>(x)();
                  return bp::object(PyCF(UnaryOpCF(coef.Get(), func, func, FUNC::Name())));
                }
              bp::extract<double> ed(x);
              if (ed.check()) return bp::object(func(ed()));
              if (bp::extract<Complex> (x).check())
                return bp::object(func(bp::extract<Complex> (x)()));
              throw Exception ("can't compute math-function");
            }));
}





struct GenericBSpline {
  shared_ptr<BSpline> sp;
  GenericBSpline( const BSpline &asp ) : sp(make_shared<BSpline>(asp)) {;}
  GenericBSpline( shared_ptr<BSpline> asp ) : sp(asp) {;}
  template <typename T> T operator() (T x) const { return (*sp)(x); }
  Complex operator() (Complex x) const { return (*sp)(x.real()); }
  SIMD<double> operator() (SIMD<double> x) const
  { return SIMD<double>([&](int i)->double { return (*sp)(x[i]); } );}
};
struct GenericSin {
  template <typename T> T operator() (T x) const { return sin(x); }
  static string Name() { return "sin"; }
};
struct GenericCos {
  template <typename T> T operator() (T x) const { return cos(x); }
  static string Name() { return "cos"; }
};
struct GenericTan {
  template <typename T> T operator() (T x) const { return tan(x); }
  static string Name() { return "tan"; }
};
struct GenericExp {
  template <typename T> T operator() (T x) const { return exp(x); }
  static string Name() { return "exp"; }
};
struct GenericLog {
  template <typename T> T operator() (T x) const { return log(x); }
  static string Name() { return "log"; }
};
struct GenericATan {
  template <typename T> T operator() (T x) const { return atan(x); }
  static string Name() { return "atan"; }
};
struct GenericSqrt {
  template <typename T> T operator() (T x) const { return sqrt(x); }
  static string Name() { return "sqrt"; }
};
struct GenericConj {
  template <typename T> T operator() (T x) const { return Conj(x); } // from bla
  static string Name() { return "conj"; }
  SIMD<double> operator() (SIMD<double> x) const { return x; }
  AutoDiff<1> operator() (AutoDiff<1> x) const { throw Exception ("Conj(..) is not complex differentiable"); }
  AutoDiffDiff<1> operator() (AutoDiffDiff<1> x) const { throw Exception ("Conj(..) is not complex differentiable"); }
};


  template <int D>
  class NormalVectorCF : public CoefficientFunction
  {
  public:
    NormalVectorCF () : CoefficientFunction(D,false) { ; }
    // virtual int Dimension() const { return D; }

    virtual double Evaluate (const BaseMappedIntegrationPoint & ip) const 
    {
      return 0;
    }
    virtual void Evaluate (const BaseMappedIntegrationPoint & ip, FlatVector<> res) const 
    {
      if (ip.Dim() != D)
        throw Exception("illegal dim of normal vector");
      res = static_cast<const DimMappedIntegrationPoint<D>&>(ip).GetNV();
    }

    virtual void Evaluate (const BaseMappedIntegrationRule & ir, FlatMatrix<> res) const 
    {
      if (ir[0].Dim() != D)
        throw Exception("illegal dim of normal vector");
      FlatMatrixFixWidth<D> resD(res);
      for (int i = 0; i < ir.Size(); i++)
        resD.Row(i) = static_cast<const DimMappedIntegrationPoint<D>&>(ir[i]).GetNV();
    }
    virtual void Evaluate (const BaseMappedIntegrationRule & ir, FlatMatrix<Complex> res) const 
    {
      if (ir[0].Dim() != D)
        throw Exception("illegal dim of normal vector");
      for (int i = 0; i < ir.Size(); i++)
        res.Row(i) = static_cast<const DimMappedIntegrationPoint<D>&>(ir[i]).GetNV();
    }

    virtual void GenerateCode(Code &code, FlatArray<int> inputs, int index) const {
        string miptype;
        if(code.is_simd)
          miptype = "SIMD<DimMappedIntegrationPoint<"+ToString(D)+">>*";
        else
          miptype = "DimMappedIntegrationPoint<"+ToString(D)+">*";
        auto nv_expr = CodeExpr("static_cast<const "+miptype+">(&ip)->GetNV()");
        auto nv = Var("tmp", index);
        code.body += nv.Assign(nv_expr);
        for( int i : Range(D))
          code.body += Var(index,i).Assign(nv(i));
    }

    virtual void Evaluate (const SIMD_BaseMappedIntegrationRule & ir, ABareSliceMatrix<double> values) const
    {
      for (size_t i = 0; i < ir.Size(); i++)
        for (size_t j = 0; j < D; j++)
          values.Get(j,i) = static_cast<const SIMD<DimMappedIntegrationPoint<D>>&>(ir[i]).GetNV()(j).Data();
    }

    virtual void Evaluate (const SIMD_BaseMappedIntegrationRule & ir, FlatArray<AFlatMatrix<double>*> input,
                           AFlatMatrix<double> values) const
    {
      Evaluate (ir, values);
    }
    
  };

  template <int D>
  class TangentialVectorCF : public CoefficientFunction
  {
  public:
    TangentialVectorCF () : CoefficientFunction(D,false) { ; }
    // virtual int Dimension() const { return D; }

    virtual double Evaluate (const BaseMappedIntegrationPoint & ip) const 
    {
      return 0;
    }
    virtual void Evaluate (const BaseMappedIntegrationPoint & ip, FlatVector<> res) const 
    {
      if (ip.Dim() != D)
        throw Exception("illegal dim of tangential vector");
      res = static_cast<const DimMappedIntegrationPoint<D>&>(ip).GetTV();
    }
  };


  class CoordCoefficientFunction : public CoefficientFunction
  {
    int dir;
  public:
    CoordCoefficientFunction (int adir) : CoefficientFunction(1, false), dir(adir) { ; }
    virtual double Evaluate (const BaseMappedIntegrationPoint & ip) const 
    {
      return ip.GetPoint()(dir);
    }
    virtual void Evaluate(const BaseMappedIntegrationRule & ir,
                          FlatMatrix<> result) const
    {
      result.Col(0) = ir.GetPoints().Col(dir);
      /*
      for (int i = 0; i < ir.Size(); i++)
        result(i,0) = ir[i].GetPoint()(dir);
      */
    }
    virtual void Evaluate(const BaseMappedIntegrationRule & ir,
                          FlatMatrix<Complex> result) const
    {
      result.Col(0) = ir.GetPoints().Col(dir);
    }

    virtual void GenerateCode(Code &code, FlatArray<int> inputs, int index) const {
        auto v = Var(index);
        if(dir==0) code.body += v.Assign(CodeExpr("ip.GetPoint()(0)"));
        if(dir==1) code.body += v.Assign(CodeExpr("ip.GetPoint()(1)"));
        if(dir==2) code.body += v.Assign(CodeExpr("ip.GetPoint()(2)"));
    }

    virtual void Evaluate (const SIMD_BaseMappedIntegrationRule & ir, ABareSliceMatrix<double> values) const
    {
      auto points = ir.GetPoints();
      for (int i = 0; i < ir.Size(); i++)
        values.Get(i) = points.Get(i, dir);
    }
    virtual void Evaluate (const SIMD_BaseMappedIntegrationRule & ir, FlatArray<AFlatMatrix<double>*> input,
                           AFlatMatrix<double> values) const
    {
      Evaluate (ir, values);
    }
    
  };



void ExportCoefficientFunction()
{
  bp::class_<PyWrapper<CoefficientFunction>>
    ("CoefficientFunction",
     "A CoefficientFunction (CF) is some function defined on a mesh.\n"
     "examples are coordinates x, y, z, domain-wise constants, solution-fields, ...\n"
     "CFs can be combined by mathematical operations (+,-,sin(), ...) to form new CFs"
    )

    .def("__init__", bp::make_constructor(
         FunctionPointer ([](bp::object val, bp::object dims) //-> PyCF*
                           {
                             auto coef = new PyCF(MakeCoefficient(val));
                             if (dims)
                               {
                                 Array<int> cdims = makeCArray<int> (dims);
                                 // dynamic_pointer_cast<VectorialCoefficientFunction> (coef->Get())->SetDimensions(cdims);
                                 coef->Get()->SetDimensions(cdims);
                               }
                             return coef;
                           }),
          bp::default_call_policies(),        // need it to use named arguments
          (bp::arg("coef"),bp::arg("dims")=bp::object())),
         "Construct a CoefficientFunction from either one of\n"
         "  a scalar (float or complex)\n"
         "  a tuple of scalars and or CFs to define a vector-valued CF\n"
         "     use dims=(h,w) to define matrix-valued CF\n"
         "  a list of scalars and or CFs to define a domain-wise CF"
         )
    .def("__str__", FunctionPointer( [](PyCF self) { return ToString<CoefficientFunction>(*self.Get());}))

    .def("__call__", FunctionPointer
	 ([] (PyCF self_wrapper, BaseMappedIntegrationPoint & mip) -> bp::object
	  {
            CF & self = *self_wrapper.Get();
	    if (!self.IsComplex())
	      {
                if (self.Dimension() == 1)
                  return bp::object(self.Evaluate(mip));
                Vector<> vec(self.Dimension());
                self.Evaluate (mip, vec);
                return bp::tuple(vec);
	      }
	    else
	      {
                Vector<Complex> vec(self.Dimension());
                self.Evaluate (mip, vec);
                if (vec.Size()==1) return bp::object(vec(0));
                return bp::tuple(vec);
	      }
	  }),
         (bp::arg("coef"),bp::arg("mip")),
         "evaluate CF at a mapped integrationpoint mip. mip can be generated by calling mesh(x,y,z)")
    .add_property("dim",
        FunctionPointer( [] (PyCF self) { return self->Dimension(); } ),
                  "number of components of CF")

    .add_property("dims",
        FunctionPointer( [] (PyCF self) { return self->Dimensions(); } ),
                  "shape of CF:  (dim) for vector, (h,w) for matrix")    
    
    .def("__getitem__", FunctionPointer( [](PyCF self, int comp) -> PyCF
                                         {
                                           if (comp < 0 || comp >= self->Dimension())
                                             bp::exec("raise IndexError()\n");
                                           return PyCF(MakeComponentCoefficientFunction (self.Get(), comp));
                                         }),
         (bp::arg("coef"),bp::arg("comp")),         
         "returns component comp of vectorial CF")
    .def("__getitem__", FunctionPointer( [](PyCF self, bp::tuple comps) -> PyCF
                                         {
                                           if (bp::len(comps) != 2)
                                             bp::exec("raise IndexError()\n");
                                           FlatArray<int> dims = self->Dimensions();
                                           if (dims.Size() != 2)
                                             bp::exec("raise IndexError()\n");
                                           
                                           int c1 = bp::extract<int> (comps[0]);
                                           int c2 = bp::extract<int> (comps[1]);
                                           if (c1 < 0 || c2 < 0 || c1 >= dims[0] || c2 >= dims[1])
                                             bp::exec("raise IndexError()\n");

                                           int comp = c1 * dims[1] + c2;
                                           return PyCF(MakeComponentCoefficientFunction (self.Get(), comp));
                                         }))

    // coefficient expressions
    .def ("__add__", FunctionPointer 
          ([] (PyCF c1, PyCF c2) -> PyCF { return c1.Get()+c2.Get(); } ))
    .def ("__add__", FunctionPointer 
          ([] (PyCF coef, double val) -> PyCF
           {
             return coef.Get() + make_shared<ConstantCoefficientFunction>(val);
           }))
    .def ("__radd__", FunctionPointer 
          ([] (PyCF coef, double val) -> PyCF
           { return coef.Get() + make_shared<ConstantCoefficientFunction>(val); }))

    .def ("__sub__", FunctionPointer 
          ([] (PyCF c1, PyCF c2) -> PyCF
           { return c1.Get()-c2.Get(); }))

    .def ("__sub__", FunctionPointer 
          ([] (PyCF coef, double val) -> PyCF
           { return coef.Get() - make_shared<ConstantCoefficientFunction>(val); }))

    .def ("__rsub__", FunctionPointer 
          ([] (PyCF coef, double val) -> PyCF
           { return make_shared<ConstantCoefficientFunction>(val) - coef.Get(); }))

    .def ("__mul__", FunctionPointer 
          ([] (PyCF c1, PyCF c2) -> PyCF
           {
             return c1.Get()*c2.Get();
           } ))

    .def ("InnerProduct", FunctionPointer
          ([] (PyCF c1, PyCF c2) -> PyCF
           { 
             return InnerProduct (c1.Get(), c2.Get());
           }))
          
    .def("Norm", FunctionPointer ( [](PyCF x) -> PyCF { return NormCF(x.Get()); }))

    /*
      // it's using the complex functions anyway ...
    .def ("__mul__", FunctionPointer 
          ([] (PyCF coef, double val) -> PyCF
           { 
             return make_shared<ScaleCoefficientFunction> (val, coef); 
           }))
    .def ("__rmul__", FunctionPointer 
          ([] (PyCF coef, double val) -> PyCF
           { return make_shared<ScaleCoefficientFunction> (val, coef); }))
    */
    .def ("__mul__", FunctionPointer 
          ([] (PyCF coef, Complex val) -> PyCF
           { 
             if (val.imag() == 0)
               return val.real() * coef.Get();
             else
               return val * coef.Get();
           }))
    .def ("__rmul__", FunctionPointer 
          ([] (PyCF coef, Complex val) -> PyCF
           { 
             if (val.imag() == 0)
               return val.real() * coef.Get();
             else
               return val * coef.Get();
           }))

    .def ("__truediv__", FunctionPointer 
          ([] (PyCF coef, PyCF coef2) -> PyCF
           { return coef.Get()/coef2.Get();
           }))

    .def ("__truediv__", FunctionPointer 
          ([] (PyCF coef, double val) -> PyCF
           { return coef.Get() / make_shared<ConstantCoefficientFunction>(val); }))

    .def ("__rtruediv__", FunctionPointer 
          ([] (PyCF coef, double val) -> PyCF
           { return make_shared<ConstantCoefficientFunction>(val) / coef.Get(); }))

    .def ("__neg__", FunctionPointer 
          ([] (PyCF coef) -> PyCF
           { return -1.0 * coef.Get(); }))

    .add_property ("trans", FunctionPointer
                   ([] (PyCF coef) -> PyCF
                    {
                      return TransposeCF(coef.Get());
                    }),
                   "transpose of matrix-valued CF")

    .def ("Compile", FunctionPointer
          ([] (PyCF coef, bool realcompile) -> PyCF
           { return Compile (coef.Get(), realcompile); }),
          (bp::args("self"), bp::args("realcompile")=false),
          "compile list of individual steps, experimental improvement for deep trees")
    ;

  ExportStdMathFunction<GenericSin>("sin");
  ExportStdMathFunction<GenericCos>("cos");
  ExportStdMathFunction<GenericTan>("tan");
  ExportStdMathFunction<GenericExp>("exp");
  ExportStdMathFunction<GenericLog>("log");
  ExportStdMathFunction<GenericATan>("atan");
  ExportStdMathFunction<GenericSqrt>("sqrt");
  ExportStdMathFunction<GenericConj>("Conj");
  
  bp::def ("IfPos", FunctionPointer 
           ([] (PyCF c1, bp::object then_obj, bp::object else_obj) -> PyCF
            {
              return IfPos(c1.Get(),
                           MakeCoefficient(then_obj).Get(),
                           MakeCoefficient(else_obj).Get());
            } ))
    ;
  
  typedef PyWrapperDerived<ConstantCoefficientFunction, CoefficientFunction> PyConstCF;
  bp::class_<PyConstCF,bp::bases<PyCF>>
    ("ConstantCF", "same as CoefficientFunction(c), obsolete")
     .def("__init__", bp::make_constructor
          (FunctionPointer ([](double value) -> PyConstCF *
                             {
                               return new PyConstCF(make_shared<ConstantCoefficientFunction>(value));
                             })))
    ;

  // bp::implicitly_convertible 
  // <shared_ptr<ConstantCoefficientFunction>, shared_ptr<CoefficientFunction> >(); 

  // REGISTER_PTR_TO_PYTHON_BOOST_1_60_FIX(shared_ptr<ParameterCoefficientFunction>);
  typedef PyWrapperDerived<ParameterCoefficientFunction, CoefficientFunction> PyParameterCF;
  bp::class_<PyParameterCF,bp::bases<PyCF>>
    ("Parameter", "CoefficientFunction with a modifiable value", bp::no_init)
    .def ("__init__", bp::make_constructor
          (FunctionPointer ([] (double val) -> PyParameterCF*
                            {
                              return new PyParameterCF(make_shared<ParameterCoefficientFunction>(val));
                            })))
    .def ("Set",
          FunctionPointer ([] (PyParameterCF cf, double val)
                           {
                             cf->SetValue (val);
                           }),
          "modify parameter value")
    ;

  // bp::implicitly_convertible 
  // <shared_ptr<ParameterCoefficientFunction>, shared_ptr<CoefficientFunction> >(); 

  
  class CoordCoefficientFunction : public CoefficientFunction
  {
    int dir;
  public:
    CoordCoefficientFunction (int adir) : CoefficientFunction(1, false), dir(adir) { ; }
    virtual double Evaluate (const BaseMappedIntegrationPoint & ip) const 
    {
      return ip.GetPoint()(dir);
    }
    virtual void Evaluate(const BaseMappedIntegrationRule & ir,
                          FlatMatrix<> result) const
    {
      result.Col(0) = ir.GetPoints().Col(dir);
      /*
      for (int i = 0; i < ir.Size(); i++)
        result(i,0) = ir[i].GetPoint()(dir);
      */
    }
    virtual void Evaluate(const BaseMappedIntegrationRule & ir,
                          FlatMatrix<Complex> result) const
    {
      result.Col(0) = ir.GetPoints().Col(dir);
    }

    virtual void GenerateCode(Code &code, FlatArray<int> inputs, int index) const {
        auto v = Var(index);
        if(dir==0) code.body += v.Assign(CodeExpr("ip.GetPoint()(0)"));
        if(dir==1) code.body += v.Assign(CodeExpr("ip.GetPoint()(1)"));
        if(dir==2) code.body += v.Assign(CodeExpr("ip.GetPoint()(2)"));
    }

    virtual void Evaluate (const SIMD_BaseMappedIntegrationRule & ir, ABareSliceMatrix<double> values) const
    {
      auto points = ir.GetPoints();
      for (int i = 0; i < ir.Size(); i++)
        values.Get(i) = points.Get(i, dir);
    }
    virtual void Evaluate (const SIMD_BaseMappedIntegrationRule & ir, FlatArray<AFlatMatrix<double>*> input,
                           AFlatMatrix<double> values) const
    {
      Evaluate (ir, values);
    }
    
  };

  typedef PyWrapperDerived<CoordCoefficientFunction, CoefficientFunction> PyCoordCF;
  bp::class_<PyCoordCF,bp::bases<PyCF>>
    ("CoordCF", "CoefficientFunction for x, y, z")
     .def("__init__", bp::make_constructor
          (FunctionPointer ([](int direction) -> PyCoordCF *
                             {
                               return new PyCoordCF(make_shared<CoordCoefficientFunction>(direction));
                             })))
    ;

  class MeshSizeCF : public CoefficientFunction
  {
  public:
    MeshSizeCF () : CoefficientFunction(1, false) { ; }
    virtual double Evaluate (const BaseMappedIntegrationPoint & ip) const 
    {
      if (ip.IP().FacetNr() != -1) // on a boundary facet of the element
        {
          double det = 1;
          switch (ip.Dim())
            {
            case 1: det = fabs (static_cast<const MappedIntegrationPoint<1,1>&> (ip).GetJacobiDet()); break;
            case 2: det = fabs (static_cast<const MappedIntegrationPoint<2,2>&> (ip).GetJacobiDet()); break;
            case 3: det = fabs (static_cast<const MappedIntegrationPoint<3,3>&> (ip).GetJacobiDet()); break;
            default:
              throw Exception("Illegal dimension in MeshSizeCF");
            }
          return det/ip.GetMeasure();
        }
      
      switch (ip.Dim())
        {
        case 1: return fabs (static_cast<const MappedIntegrationPoint<1,1>&> (ip).GetJacobiDet());
        case 2: return pow (fabs (static_cast<const MappedIntegrationPoint<2,2>&> (ip).GetJacobiDet()), 1.0/2);
        case 3:
        default:
          return pow (fabs (static_cast<const MappedIntegrationPoint<3,3>&> (ip).GetJacobiDet()), 1.0/3);
        }
      // return pow(ip.GetMeasure(), 1.0/(ip.Dim());
    }

    virtual void Evaluate (const SIMD_BaseMappedIntegrationRule & ir, ABareSliceMatrix<double> values) const
    {
      if (ir[0].IP().FacetNr() != -1)
        for(size_t i : Range(ir))
          values.Get(i) =  fabs (ir[i].GetJacobiDet()) / ir[i].GetMeasure();
      else
        for(size_t i : Range(ir))
          values.Get(i) =  pow(fabs (ir[i].GetJacobiDet()), 1.0/ir.DimElement()).Data();
    }

    virtual void GenerateCode(Code &code, FlatArray<int> inputs, int index) const {
      if(code.is_simd)
        code.body += Var(index).Assign( CodeExpr("pow(ip.GetJacobiDet(), 1.0/mir.DimElement())"));
      else
      {
        code.body += Var(index).Declare( "double" );
        code.body += R"CODE_(
        {
          double tmp_res = 0.0;
          switch (ip.Dim()) {
            case 1:  tmp_res =      fabs (static_cast<const MappedIntegrationPoint<1,1>&> (ip).GetJacobiDet()); break;
            case 2:  tmp_res = pow (fabs (static_cast<const MappedIntegrationPoint<2,2>&> (ip).GetJacobiDet()), 1.0/2); break;
            default: tmp_res = pow (fabs (static_cast<const MappedIntegrationPoint<3,3>&> (ip).GetJacobiDet()), 1.0/3);
        }
        )CODE_" + Var(index).S() + " = tmp_res;\n}\n;";
      }
    }
  };


  class SpecialCoefficientFunctions
  {
  public:
    PyCF GetMeshSizeCF ()
    { return PyCF(make_shared<MeshSizeCF>()); }

    PyCF GetNormalVectorCF (int dim)
    { 
      switch(dim)
	{
	case 1:
	  return PyCF(make_shared<NormalVectorCF<1>>());
	case 2:
	  return PyCF(make_shared<NormalVectorCF<2>>());
	default:
	  return PyCF(make_shared<NormalVectorCF<3>>());
	}
    }

    PyCF GetTangentialVectorCF (int dim)
    { 
      switch(dim)
	{
	case 1:
	  return PyCF(make_shared<TangentialVectorCF<1>>());
	case 2:
	  return PyCF(make_shared<TangentialVectorCF<2>>());
	default:
	  return PyCF(make_shared<TangentialVectorCF<3>>());
	}
    }
  };

  bp::class_<SpecialCoefficientFunctions> ("SpecialCFCreator", bp::no_init)
    .add_property("mesh_size", 
                  &SpecialCoefficientFunctions::GetMeshSizeCF, "local mesh-size (approximate element diameter) as CF")
    .def("normal", &SpecialCoefficientFunctions::GetNormalVectorCF,
         "depending on contents: normal-vector to geometry or element\n"
         "space-dimension must be provided")
    .def("tangential", &SpecialCoefficientFunctions::GetTangentialVectorCF,
         "depending on contents: tangential-vector to element\n"
         "space-dimension must be provided")
    ;
  static SpecialCoefficientFunctions specialcf;
  
  bp::scope().attr("specialcf") = bp::object(bp::ptr(&specialcf));

  bp::class_<BSpline, shared_ptr<BSpline> > ("BSpline", bp::no_init)
   .def("__init__", bp::make_constructor 
        (FunctionPointer ([](int order, bp::list knots, bp::list vals)
                           {
                             return make_shared<BSpline> (order,
                                                 makeCArray<double> (knots),
                                                 makeCArray<double> (vals));
                           })),
        "B-Spline of a certain order, provide knot and value vectors")
    .def("__str__", &ToString<BSpline>)
    .def("__call__", &BSpline::Evaluate)
    .def("__call__", FunctionPointer
         ([](shared_ptr<BSpline> sp, PyCF coef) -> PyCF
          {
            return UnaryOpCF (coef.Get(), GenericBSpline(sp), GenericBSpline(sp));
          }))
    .def("Integrate", 
         FunctionPointer([](const BSpline & sp) { return make_shared<BSpline>(sp.Integrate()); }))
    .def("Differentiate", 
         FunctionPointer([](const BSpline & sp) { return make_shared<BSpline>(sp.Differentiate()); }))
    ;
}



MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::BaseScalarFiniteElement)
MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::BilinearFormIntegrator)
MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::BlockBilinearFormIntegrator)
MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::BlockLinearFormIntegrator)
MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::CoefficientFunction)
MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::CompoundBilinearFormIntegrator)
MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::CompoundLinearFormIntegrator)
MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::ConstantCoefficientFunction)
MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::ParameterCoefficientFunction)
MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::DifferentialOperator)
MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::DomainConstantCoefficientFunction)
MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::DomainVariableCoefficientFunction)
MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::ElementTransformation)
MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::FiniteElement)
MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::LinearFormIntegrator)
MSVC2015_UPDATE3_GET_PTR_FIX(ngfem::ProxyFunction)
MSVC2015_UPDATE3_GET_PTR_FIX(CoordCoefficientFunction)


// *************************************** Export FEM ********************************


void NGS_DLL_HEADER ExportNgfem() {

  bp::docstring_options local_docstring_options(true, true, false);
  
    std::string nested_name = "fem";
    if( bp::scope() )
      nested_name = bp::extract<std::string>(bp::scope().attr("__name__") + ".fem");

    bp::object module(bp::handle<>(bp::borrowed(PyImport_AddModule(nested_name.c_str()))));

    cout << IM(1) << "exporting fem as " << nested_name << endl;
    bp::object parent = bp::scope() ? bp::scope() : bp::import("__main__");
    parent.attr("fem") = module ;

    bp::scope ngbla_scope(module);


  bp::enum_<ELEMENT_TYPE>("ET")
    .value("POINT", ET_POINT)     .value("SEGM", ET_SEGM)
    .value("TRIG", ET_TRIG)       .value("QUAD", ET_QUAD)
    .value("TET", ET_TET)         .value("PRISM", ET_PRISM)
    .value("PYRAMID", ET_PYRAMID) .value("HEX", ET_HEX)
    .export_values()
    ;

  bp::enum_<NODE_TYPE>("NODE_TYPE")
    .value("VERTEX", NT_VERTEX)
    .value("EDGE", NT_EDGE)
    .value("FACE", NT_FACE)
    .value("CELL", NT_CELL)
    .value("ELEMENT", NT_ELEMENT)
    .value("FACET", NT_FACET)
    .export_values()
    ;


  bp::class_<ElementTopology> ("ElementTopology", bp::init<ELEMENT_TYPE>())
    .add_property("name", 
                  static_cast<const char*(ElementTopology::*)()> (&ElementTopology::GetElementName))
    .add_property("vertices", FunctionPointer([](ElementTopology & self)
                                              {
                                                bp::list verts;
                                                const POINT3D * pts = self.GetVertices();
                                                int dim = self.GetSpaceDim();
                                                for (int i : Range(self.GetNVertices()))
                                                  {
                                                    bp::list v;
                                                    for (int j = 0; j < dim; j++)
                                                      v.append(pts[i][j]);
                                                    verts.append (bp::tuple(v));
                                                  }
                                                return verts;
                                              }));
    ;
    
  REGISTER_PTR_TO_PYTHON_BOOST_1_60_FIX(shared_ptr<FiniteElement>);
  bp::class_<FiniteElement, shared_ptr<FiniteElement>, boost::noncopyable>
    ("FiniteElement", "any finite element", bp::no_init)
    .add_property("ndof", &FiniteElement::GetNDof, "number of degrees of freedom of element")    
    .add_property("order", &FiniteElement::Order, "maximal polynomial order of element")    
    .add_property("type", &FiniteElement::ElementType, "geometric type of element")    
    .add_property("dim", &FiniteElement::Dim, "spatial dimension of element")    
    .add_property("classname", &FiniteElement::ClassName, "name of element family")  
    .def("__str__", &ToString<FiniteElement>)
    ;

  REGISTER_PTR_TO_PYTHON_BOOST_1_60_FIX(shared_ptr<BaseScalarFiniteElement>);
  bp::class_<BaseScalarFiniteElement, shared_ptr<BaseScalarFiniteElement>, 
    bp::bases<FiniteElement>, boost::noncopyable>
      ("ScalarFE", "a scalar-valued finite element", bp::no_init)
    .def("CalcShape",
         FunctionPointer
         ([] (const BaseScalarFiniteElement & fe, double x, double y, double z)
          {
            IntegrationPoint ip(x,y,z);
            Vector<> v(fe.GetNDof());
            fe.CalcShape (ip, v);
            return v;
          }),
         (bp::arg("self"),bp::arg("x"),bp::arg("y")=0.0,bp::arg("z")=0.0)
         )
    .def("CalcShape",
         FunctionPointer
         ([] (const BaseScalarFiniteElement & fe, const BaseMappedIntegrationPoint & mip)
          {
            Vector<> v(fe.GetNDof());
            fe.CalcShape (mip.IP(), v);
            return v;
          }),
         (bp::arg("self"),bp::arg("mip"))
         )
    .def("CalcDShape",
         FunctionPointer
         ([] (const BaseScalarFiniteElement & fe, const BaseMappedIntegrationPoint & mip)
          {
            Matrix<> mat(fe.GetNDof(), fe.Dim());
            switch (fe.Dim())
              {
              case 1:
                dynamic_cast<const ScalarFiniteElement<1>&> (fe).
                  CalcMappedDShape(static_cast<const MappedIntegrationPoint<1,1>&> (mip), mat); break;
              case 2:
                dynamic_cast<const ScalarFiniteElement<2>&> (fe).
                  CalcMappedDShape(static_cast<const MappedIntegrationPoint<2,2>&> (mip), mat); break;
              case 3:
                dynamic_cast<const ScalarFiniteElement<3>&> (fe).
                  CalcMappedDShape(static_cast<const MappedIntegrationPoint<3,3>&> (mip), mat); break;
              default:
                ;
              }
            return mat;
          }),
         (bp::arg("self"),bp::arg("mip"))
         )
    ;



  bp::implicitly_convertible 
    <shared_ptr<BaseScalarFiniteElement>, 
    shared_ptr<FiniteElement> >(); 


  bp::def("H1FE", FunctionPointer
          ([](ELEMENT_TYPE et, int order)
           {
             BaseScalarFiniteElement * fe = nullptr;
             switch (et)
               {
               case ET_TRIG: fe = new H1HighOrderFE<ET_TRIG>(order); break;
               case ET_QUAD: fe = new H1HighOrderFE<ET_QUAD>(order); break;
               case ET_TET: fe = new H1HighOrderFE<ET_TET>(order); break;
               default: cerr << "cannot make fe " << et << endl;
               }
             return shared_ptr<BaseScalarFiniteElement>(fe);
           }),
          "creates an H1 finite element of given geometric shape and polynomial order"
          );

  bp::def("L2FE", FunctionPointer
          ([](ELEMENT_TYPE et, int order)
           {
             BaseScalarFiniteElement * fe = nullptr;
             switch (et)
               {
               case ET_TRIG: fe = new L2HighOrderFE<ET_TRIG>(order); break;
               case ET_QUAD: fe = new L2HighOrderFE<ET_QUAD>(order); break;
               case ET_TET: fe = new L2HighOrderFE<ET_TET>(order); break;
               default: cerr << "cannot make fe " << et << endl;
               }
             return shared_ptr<BaseScalarFiniteElement>(fe);
           })
          );


  bp::class_<IntegrationPoint>("IntegrationPoint", bp::no_init);

  bp::class_<IntegrationRule, boost::noncopyable>("IntegrationRule", bp::no_init)
    .def("__init__",  bp::make_constructor 
         (FunctionPointer ([](ELEMENT_TYPE et, int order) -> IntegrationRule *
                           {
                             return new IntegrationRule (et, order);
                           }),
          bp::default_call_policies(),
          (bp::arg("element type"), bp::arg("order"))))
    .def("__getitem__", FunctionPointer([](IntegrationRule & ir, int nr)->IntegrationPoint
                                        {
                                          if (nr < 0 || nr >= ir.Size())
                                            bp::exec("raise IndexError()\n");
                                          return ir[nr];
                                        }))
    .def("Integrate", FunctionPointer
         ([](IntegrationRule & ir, bp::object func) -> bp::object
          {
            bp::object sum;
            for (const IntegrationPoint & ip : ir)
              {
                bp::object val;
                switch (ir.Dim())
                  {
                  case 1:
                    val = func(ip(0)); break;
                  case 2:
                    val = func(ip(0), ip(1)); break;
                  case 3:
                    val = func(ip(0), ip(1), ip(2)); break;
                  default:
                    throw Exception("integration rule with illegal dimension");
                  }

                val = val * ip.Weight();
                if (sum == bp::object())
                  sum = val;
                else
                  sum = sum+val;
              }
            return sum;
          }))
    ;

  bp::class_<BaseMappedIntegrationPoint, boost::noncopyable>( "BaseMappedIntegrationPoint", bp::no_init)
    .def("__str__", FunctionPointer
         ([] (const BaseMappedIntegrationPoint & bmip)
          {
            stringstream str;
            str << "p = " << bmip.GetPoint() << endl;
            str << "jac = " << bmip.GetJacobian() << endl;
            /*
            switch (bmip.Dim())
              {
              case 1: 
                {
                  auto & mip = static_cast<const MappedIntegrationPoint<1,1>&>(bmip);
                  str << "jac = " << mip.GetJacobian() << endl;
                  break;
                }
              case 2: 
                {
                  auto & mip = static_cast<const MappedIntegrationPoint<2,2>&>(bmip);
                  str << "jac = " << mip.GetJacobian() << endl;
                  break;
                }
              case 3: 
                {
                  auto & mip = static_cast<const MappedIntegrationPoint<3,3>&>(bmip);
                  str << "jac = " << mip.GetJacobian() << endl;
                  break;
                }
              default:
                ;
              }
            */
            str << "measure = " << bmip.GetMeasure() << endl;
            return str.str();
          }))
    .add_property("measure", &BaseMappedIntegrationPoint::GetMeasure)
    .add_property("point", &BaseMappedIntegrationPoint::GetPoint)
    .add_property("jacobi", &BaseMappedIntegrationPoint::GetJacobian)
    // .add_property("trafo", &BaseMappedIntegrationPoint::GetTransformation)
    .add_property("trafo", bp::make_function( &BaseMappedIntegrationPoint::GetTransformation,
                                              bp::return_value_policy<bp::reference_existing_object>()))
    .add_property("elementid", FunctionPointer([](BaseMappedIntegrationPoint & mip)
                                               {
                                                 return mip.GetTransformation().GetElementId();
                                               }))
    ;

    
  bp::class_<ElementTransformation, boost::noncopyable>("ElementTransformation", bp::no_init)
    .def("__init__", bp::make_constructor
         (FunctionPointer ([](ELEMENT_TYPE et, bp::object vertices) -> ElementTransformation*
                           {
                             int nv = ElementTopology::GetNVertices(et);
                             int dim = bp::len(vertices[0]);
                             Matrix<> pmat(nv,dim);
                             for (int i : Range(nv))
                               for (int j : Range(dim))
                                 pmat(i,j) = bp::extract<double> (vertices[i][j])();
                             switch (Dim(et))
                               {
                               case 1:
                                 return new FE_ElementTransformation<1,1> (et, pmat); 
                               case 2:
                                 return new FE_ElementTransformation<2,2> (et, pmat); 
                               case 3:
                                 return new FE_ElementTransformation<3,3> (et, pmat); 
                               default:
                                 throw Exception ("cannot create ElementTransformation");
                               }
                           }),
          bp::default_call_policies(),        // need it to use named arguments
          (bp::arg("et")=ET_TRIG,bp::arg("vertices"))))
    .add_property("VB", &ElementTransformation::VB)
    .add_property("spacedim", &ElementTransformation::SpaceDim)
    .add_property("elementid", &ElementTransformation::GetElementId)
    .def ("__call__", FunctionPointer
          ([] (ElementTransformation & self, double x, double y, double z)
           {
             
             return &self(IntegrationPoint(x,y,z), global_alloc);
           }),
          (bp::args("self"), bp::args("x"), bp::args("y")=0, bp::args("z")=0),
          bp::return_value_policy<bp::manage_new_object>())
    .def ("__call__", FunctionPointer
          ([] (ElementTransformation & self, IntegrationPoint & ip)
           {
             return &self(ip, global_alloc);
           }),
          (bp::args("self"), bp::args("ip")),
          bp::return_value_policy<bp::manage_new_object>())
    ;


  REGISTER_PTR_TO_PYTHON_BOOST_1_60_FIX(shared_ptr<DifferentialOperator>);
  bp::class_<DifferentialOperator, shared_ptr<DifferentialOperator>, boost::noncopyable>
    ("DifferentialOperator", bp::no_init)
    ;

  
  typedef PyWrapper<BilinearFormIntegrator> PyBFI;
  bp::class_<PyBFI>
    ("BFI", bp::no_init)
    .def("__init__", bp::make_constructor
         (FunctionPointer ([](string name, int dim, bp::object py_coef, bp::object definedon, bool imag,
                              string filename, Flags flags)
                           {
                             Array<shared_ptr<CoefficientFunction>> coef = MakeCoefficients(py_coef);
                             auto bfi = GetIntegrators().CreateBFI (name, dim, coef);

                             if (!bfi) cerr << "undefined integrator '" << name 
                                            << "' in " << dim << " dimension" << endl;

                             if (bp::extract<bp::list> (definedon).check())
                               {
                                 Array<int> defon = makeCArray<int> (definedon);
                                 for (int & d : defon) d--;
                                 bfi -> SetDefinedOn (defon); 
                               }
                             else if (bp::extract<BitArray> (definedon).check())
                               bfi -> SetDefinedOn (bp::extract<BitArray> (definedon)());
                             else if (definedon != bp::object())
                               throw Exception (string ("cannot handle definedon of type <todo>"));

                             if (filename.length())
                               {
                                 cout << "set integrator filename: " << filename << endl;
                                 bfi -> SetFileName (filename);
                               }
                             bfi -> SetFlags (flags);
                             if (imag)
                               bfi = make_shared<ComplexBilinearFormIntegrator> (bfi, Complex(0,1));

                             return new PyBFI(bfi);
                           }),
          bp::default_call_policies(),        // need it to use named arguments
          (bp::arg("name")=NULL,bp::arg("dim")=-1,bp::arg("coef"),
           bp::arg("definedon")=bp::object(),bp::arg("imag")=false, bp::arg("filename")="", bp::arg("flags") = bp::dict())))
    
    .def("__str__", FunctionPointer( [](PyBFI self) { return ToString<BilinearFormIntegrator>(*self.Get()); } ))

    .def("Evaluator", FunctionPointer( [](PyBFI self, string name ) { return self->GetEvaluator(name); } ))
    // .def("DefinedOn", &Integrator::DefinedOn)
    .def("GetDefinedOn", FunctionPointer
         ( [] (PyBFI self) -> const BitArray &{ return self->GetDefinedOn(); } ),
         bp::return_value_policy<bp::reference_existing_object>())
    

    /*
    .def("CalcElementMatrix", 
         static_cast<void(BilinearFormIntegrator::*) (const FiniteElement&, 
                                                      const ElementTransformation&,
                                                      FlatMatrix<double>,LocalHeap&)const>
         (&BilinearFormIntegrator::CalcElementMatrix))

    .def("CalcElementMatrix",
         FunctionPointer([] (const BilinearFormIntegrator & self, 
                             const FiniteElement & fe, const ElementTransformation & trafo,
                             LocalHeap & lh)
                         {
                           Matrix<> mat(fe.GetNDof());
                           self.CalcElementMatrix (fe, trafo, mat, lh);
                           return mat;
                         }),
         bp::default_call_policies(),        // need it to use named arguments
         (bp::arg("bfi")=NULL, bp::arg("fel"),bp::arg("trafo"),bp::arg("localheap")))
    */

    .def("CalcElementMatrix",
         FunctionPointer([] (PyBFI self,
                             const FiniteElement & fe, const ElementTransformation & trafo,
                             int heapsize)
                         {
                           Matrix<> mat(fe.GetNDof());
                           while (true)
                             {
                               try
                                 {
                                   LocalHeap lh(heapsize);
                                   self->CalcElementMatrix (fe, trafo, mat, lh);
                                   return mat;
                                 }
                               catch (LocalHeapOverflow ex)
                                 {
                                   heapsize *= 10;
                                 }
                             };
                         }),
         bp::default_call_policies(),        // need it to use named arguments
         (bp::arg("bfi")=NULL, bp::arg("fel"),bp::arg("trafo"),bp::arg("heapsize")=10000))
    ;


  bp::def("CompoundBFI", 
          (FunctionPointer ([]( PyBFI bfi, int comp ) -> PyBFI
                            {
                                return make_shared<CompoundBilinearFormIntegrator>(bfi.Get(), comp);
                            })),
           bp::default_call_policies(),     // need it to use named arguments
           (bp::arg("bfi")=NULL, bp::arg("comp")=0)
      );

  bp::def("BlockBFI", 
          (FunctionPointer ([]( PyBFI bfi, int dim, int comp ) -> PyBFI
                            {
                                return make_shared<BlockBilinearFormIntegrator>(bfi.Get(), dim, comp);
                            })),
           bp::default_call_policies(),     // need it to use named arguments
           (bp::arg("bfi")=NULL, bp::arg("dim")=2, bp::arg("comp")=0)
      )
      ;


  bp::class_<PyWrapperDerived<CompoundBilinearFormIntegrator, BilinearFormIntegrator>,bp::bases<PyBFI>>
      ("CompoundBilinearFormIntegrator", bp::no_init);

  bp::class_<PyWrapperDerived<BlockBilinearFormIntegrator, BilinearFormIntegrator>,bp::bases<PyBFI>>
      ("BlockBilinearFormIntegrator", bp::no_init);


  typedef PyWrapper<LinearFormIntegrator> PyLFI;
  bp::class_<PyLFI>
    ("LFI", bp::no_init)
    .def("__init__", bp::make_constructor
         (FunctionPointer ([](string name, int dim, 
                              bp::object py_coef,
                              bp::object definedon, bool imag, const Flags & flags)
                           {
                             Array<shared_ptr<CoefficientFunction>> coef = MakeCoefficients(py_coef);
                             auto lfi = GetIntegrators().CreateLFI (name, dim, coef);

                             if (!lfi) throw Exception(string("undefined integrator '")+name+
                                                       "' in "+ToString(dim)+ " dimension having 1 coefficient");

                             
                             if (bp::extract<bp::list> (definedon).check())
                               lfi -> SetDefinedOn (makeCArray<int> (definedon));
 
                             if (imag)
                               lfi = make_shared<ComplexLinearFormIntegrator> (lfi, Complex(0,1));

                             return new PyLFI(lfi);
                           }),
          bp::default_call_policies(),     // need it to use named arguments
          (bp::arg("name")=NULL,bp::arg("dim")=-1,
           bp::arg("coef"),bp::arg("definedon")=bp::object(), 
           bp::arg("imag")=false, bp::arg("flags")=bp::dict()))
         )

    /*
    .def("__init__", bp::make_constructor
         (FunctionPointer ([](string name, int dim, bp::list coefs_list,
                              bp::object definedon, bool imag, const Flags & flags)
                           {
                             Array<shared_ptr<CoefficientFunction> > coefs = makeCArray<shared_ptr<CoefficientFunction>> (coefs_list);
                             auto lfi = GetIntegrators().CreateLFI (name, dim, coefs);
                             
                             if (bp::extract<bp::list> (definedon).check())
                               lfi -> SetDefinedOn (makeCArray<int> (definedon));
 
                             if (imag)
                               lfi = make_shared<ComplexLinearFormIntegrator> (lfi, Complex(0,1));

                             // cout << "LFI: Flags = " << flags << endl;
                             if (!lfi) cerr << "undefined integrator '" << name 
                                            << "' in " << dim << " dimension having 1 coefficient"
                                            << endl;
                             return lfi;
                           }),
          bp::default_call_policies(),     // need it to use named arguments
          (bp::arg("name")=NULL,bp::arg("dim")=-1,
           bp::arg("coef"),bp::arg("definedon")=bp::object(), 
           bp::arg("imag")=false, bp::arg("flags")=bp::dict()))
        )
    */

    .def("__str__", FunctionPointer( [](PyLFI self) { return ToString<LinearFormIntegrator>(*self.Get()); } ))
    
    // .def("GetDefinedOn", &Integrator::GetDefinedOn)
    .def("GetDefinedOn", FunctionPointer
         ( [] (PyLFI self) -> const BitArray &{ return self->GetDefinedOn(); } ),
         bp::return_value_policy<bp::reference_existing_object>())

    .def("CalcElementVector", 
         static_cast<void(LinearFormIntegrator::*)(const FiniteElement&, const ElementTransformation&, FlatVector<double>,LocalHeap&)const>
         (&LinearFormIntegrator::CalcElementVector))
    .def("CalcElementVector",
         FunctionPointer([] (PyLFI  self,
                             const FiniteElement & fe, const ElementTransformation & trafo,
                             int heapsize)
                         {
                           Vector<> vec(fe.GetNDof());
                           while (true)
                             {
                               try
                                 {
                                   LocalHeap lh(heapsize);
                                   self->CalcElementVector (fe, trafo, vec, lh);
                                   return vec;
                                 }
                               catch (LocalHeapOverflow ex)
                                 {
                                   heapsize *= 10;
                                 }
                             };
                         }),
         bp::default_call_policies(),        // need it to use named arguments
         (bp::arg("lfi")=NULL, bp::arg("fel"),bp::arg("trafo"),bp::arg("heapsize")=10000))
    ;



  bp::def("CompoundLFI", 
          (FunctionPointer ([]( PyLFI lfi, int comp )
                            {
                                return PyLFI(make_shared<CompoundLinearFormIntegrator>(lfi.Get(), comp));
                            })),
           bp::default_call_policies(),     // need it to use named arguments
           (bp::arg("lfi")=NULL, bp::arg("comp")=0)
      );

  bp::def("BlockLFI", 
          (FunctionPointer ([]( PyLFI lfi, int dim, int comp )
                            {
                                return PyLFI(make_shared<BlockLinearFormIntegrator>(lfi.Get(), dim, comp));
                            })),
           bp::default_call_policies(),     // need it to use named arguments
           (bp::arg("lfi")=NULL, bp::arg("dim")=2, bp::arg("comp")=0)
      );


  bp::class_<PyWrapperDerived<CompoundLinearFormIntegrator, LinearFormIntegrator>,bp::bases<PyBFI>>
      ("CompoundLinearFormIntegrator", bp::no_init);

  bp::class_<PyWrapperDerived<BlockLinearFormIntegrator, LinearFormIntegrator>,bp::bases<PyBFI>>
      ("BlockLinearFormIntegrator", bp::no_init);


  ExportCoefficientFunction ();


  typedef PyWrapperDerived<PythonCFWrap, CoefficientFunction> PythonCFWrapWrap;
  bp::class_<PythonCFWrapWrap ,bp::bases<PyCF>>("PythonCF")
//     .def("MyEvaluate", bp::pure_virtual(static_cast<double (PythonCoefficientFunction::*)(double,double) const>(&PythonCoefficientFunction::MyEvaluate))) 
    .def("Evaluate",
        bp::pure_virtual( FunctionPointer( [] ( PythonCFWrapWrap self, const BaseMappedIntegrationPoint & ip )
        {
          return static_cast<CoefficientFunction &>(*self.Get()).Evaluate(ip);
        } )))
    .def("GetCoordinates", FunctionPointer( [] ( PythonCFWrapWrap self, const BaseMappedIntegrationPoint & ip ) 
        {
          return self->GetCoordinates(ip);
        }))
//     .def("MyEvaluate", bp::pure_virtual(&PythonCoefficientFunction::MyEvaluate)) 
    ;

  typedef PyWrapperDerived<DomainVariableCoefficientFunction, CoefficientFunction> PyDomVarCF;
  bp::class_<PyDomVarCF,bp::bases<PyCF>>
    ("VariableCF", bp::no_init)
    .def("__init__", bp::make_constructor 
         (FunctionPointer ([](string str)
                           {
                             auto ef = make_shared<EvalFunction> (str);
                             return new PyDomVarCF(make_shared<DomainVariableCoefficientFunction>
                               (Array<shared_ptr<EvalFunction>> ({ ef })));
                           })))
    ;

  typedef PyWrapperDerived<DomainConstantCoefficientFunction, CoefficientFunction> PyDomConstCF;
  bp::class_<PyDomConstCF,bp::bases<PyCF>, boost::noncopyable>
    ("DomainConstantCF", bp::no_init)
    .def("__init__", bp::make_constructor 
         (FunctionPointer ([](bp::object coefs)
                           {
                             Array<double> darray (makeCArray<double> (coefs));
                             return new PyDomConstCF(make_shared<DomainConstantCoefficientFunction> (darray));
                           })))
    ;

  bp::def ("SetPMLParameters", 
           FunctionPointer([] (double rad, double alpha)
                           {
                             cout << "set pml parameters, r = " << rad << ", alpha = " << alpha << endl;
                             constant_table_for_FEM = &pmlpar;
                             pmlpar.Set("pml_r", rad);
                             pmlpar.Set("pml_alpha", alpha);
                             SetPMLParameters();
                           }),
           (bp::arg("rad")=1,bp::arg("alpha")=1))
    ;
    
                           
                           

}


void ExportNgstd();
void ExportNgbla();

BOOST_PYTHON_MODULE(libngfem) {
  // ExportNgstd();
  // ExportNgbla();
  ExportNgfem();
}



#endif
