/*********************************************************************/
/* File:   l2hofe_segm.cpp                                           */
/* Author: Start                                                     */
/* Date:   6. Feb. 2003                                              */
/*********************************************************************/


#include <fem.hpp>
#include "l2hofefo.hpp"

namespace ngfem
{
  template class L2HighOrderFE<ET_SEGM>;  
  template class T_ScalarFiniteElement<L2HighOrderFE_Shape<ET_SEGM>, ET_SEGM, DGFiniteElement<1> >;
  
  template<>
  ScalarFiniteElement<1> * CreateL2HighOrderFE<ET_SEGM> (int order, FlatArray<int> vnums, Allocator & lh)
  {
    DGFiniteElement<1> * hofe = 0;
    switch (order)
      {
      case 0: hofe = new (lh)  L2HighOrderFEFO<ET_SEGM,0> (); break;
      case 1: hofe = new (lh)  L2HighOrderFEFO<ET_SEGM,1> (); break;
      case 2: hofe = new (lh)  L2HighOrderFEFO<ET_SEGM,2> (); break;
      case 3: hofe = new (lh)  L2HighOrderFEFO<ET_SEGM,3> (); break;
      case 4: hofe = new (lh)  L2HighOrderFEFO<ET_SEGM,4> (); break;
      case 5: hofe = new (lh)  L2HighOrderFEFO<ET_SEGM,5> (); break;
      case 6: hofe = new (lh)  L2HighOrderFEFO<ET_SEGM,6> (); break;
      case 7: hofe = new (lh)  L2HighOrderFEFO<ET_SEGM,7> (); break;      
      case 8: hofe = new (lh)  L2HighOrderFEFO<ET_SEGM,8> (); break;
      default: hofe = new (lh) L2HighOrderFE<ET_SEGM> (order); break;
      }

    for (int j = 0; j < 2; j++)
      hofe->SetVertexNumber (j, vnums[j]);
    
    return hofe;
  }
  
  
}
