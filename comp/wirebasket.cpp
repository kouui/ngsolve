#include <comp.hpp>
#include <solve.hpp>


namespace ngcomp
{
  template <class SCAL> class ElementByElementMatrix;

  template <class SCAL>
  ElementByElement_BilinearForm<SCAL> :: 
  ElementByElement_BilinearForm (const FESpace & afespace, const string & aname,
				 const Flags & flags)
    : S_BilinearForm<SCAL> (afespace, aname, flags)
  { ; }

  template <class SCAL>
  ElementByElement_BilinearForm<SCAL> :: ~ElementByElement_BilinearForm ()
  { ; }



  
  template <class SCAL>
  void ElementByElement_BilinearForm<SCAL> :: AllocateMatrix ()
  {
    cout << "alloc matrix" << endl;
    const FESpace & fespace = this->fespace;
    this->mats.Append (new ElementByElementMatrix<SCAL> (fespace.GetNDof(), this->ma.GetNE()+this->ma.GetNSE() ));
  }


  template<class SCAL>
  BaseVector * ElementByElement_BilinearForm<SCAL> :: CreateVector() const
  {
    return new VVector<SCAL> (this->fespace.GetNDof());
  }

  template<class SCAL>
  void ElementByElement_BilinearForm<SCAL> :: AddElementMatrix (const Array<int> & dnums1,
								const Array<int> & dnums2,
								const FlatMatrix<SCAL> & elmat,
								bool inner_element, int elnr,
								LocalHeap & lh)
  {
    cout << "ebe bilinearform ???" << endl;
    /*
      (*testout) << "inner_element = " << inner_element << endl;
      (*testout) << "elnr = " << elnr << endl;
      (*testout) << "elmat = " << endl << elmat << endl;
      (*testout) << "dnums1 = " << endl << dnums1 << endl;
    */
    
    dynamic_cast<ElementByElementMatrix<SCAL>&> (this->GetMatrix()) . AddElementMatrix (elnr, dnums1, dnums2, elmat);
  }
  


  template class ElementByElement_BilinearForm<double>;
  template class ElementByElement_BilinearForm<Complex>;




  
  template <class SCAL>
  ElementByElementMatrix<SCAL> :: ElementByElementMatrix (int h, int ane) 
  {
    height = h; 
    ne = ane; 
    elmats.SetSize(ne);
    dnums.SetSize(ne);
    for (int i = 0; i < ne; i++)
      {
        elmats[i].AssignMemory (0, 0, NULL);
        dnums[i] = FlatArray<int> (0, NULL);
      }
  }
  
  
  template <class SCAL>
  void ElementByElementMatrix<SCAL> :: MultAdd (double s, const BaseVector & x, BaseVector & y) const
  {
    int maxs = 0;
    for (int i = 0; i < dnums.Size(); i++)
      maxs = max2 (maxs, dnums[i].Size());

    ArrayMem<SCAL, 100> mem1(maxs), mem2(maxs);
      
    FlatVector<SCAL> vx = dynamic_cast<const S_BaseVector<SCAL> & >(x).FVScal();
    FlatVector<SCAL> vy = dynamic_cast<S_BaseVector<SCAL> & >(y).FVScal();

    for (int i = 0; i < dnums.Size(); i++)
      {
        FlatArray<int> di (dnums[i]);
        FlatVector<SCAL> hv1(di.Size(), &mem1[0]);
        FlatVector<SCAL> hv2(di.Size(), &mem2[0]);
	  
        for (int j = 0; j < di.Size(); j++)
          hv1(j) = vx (di[j]);

        hv2 = elmats[i] * hv1;
        hv2 *= s;

        for (int j = 0; j < dnums[i].Size(); j++)
          vy (di[j]) += hv2[j];
      }
  }

  template <class SCAL>
  BaseMatrix *  ElementByElementMatrix<SCAL> :: InverseMatrix ( BitArray * subset ) const
  {
    cout << "wird das tatsaechlich verwendet ???" << endl;
    ElementByElementMatrix<SCAL> * invmat = new ElementByElementMatrix<SCAL> (height, ne);

    int maxs = 0;
    for (int i = 0; i < dnums.Size(); i++)
      maxs = max2 (maxs, dnums[i].Size());

    LocalHeap lh (maxs*maxs*sizeof(SCAL)+100);

    for ( int i = 0; i < dnums.Size(); i++ )
      {
        int nd = dnums[i] . Size();
        FlatMatrix<SCAL> mat(nd, nd, lh);
        Array<int> dnumsarray(nd);
        for ( int j = 0; j < nd; j++ )
          dnumsarray[j] = dnums[i][j];
        mat = elmats[i];

        LapackInverse(mat);

        invmat -> AddElementMatrix(i, dnumsarray, dnumsarray, mat);
        lh.CleanUp();
      }
  
    return invmat;
  }
  
  template <class SCAL>
  void ElementByElementMatrix<SCAL> :: AddElementMatrix (int elnr,
                                                         const Array<int> & dnums1,
                                                         const Array<int> & dnums2,
                                                         const FlatMatrix<SCAL> & elmat)
  {
    Array<int> used;
    for (int i = 0; i < dnums1.Size(); i++)
      if (dnums1[i] >= 0) used.Append(i);
    
    int s = used.Size();

    FlatMatrix<SCAL> mat (s, new SCAL[s*s]);
    for (int i = 0; i < s; i++)
      for (int j = 0; j < s; j++)
        mat(i,j) = elmat(used[i], used[j]);

    FlatArray<int> dn(s, new int[s]);
    for (int i = 0; i < s; i++)
      dn[i] = dnums1[used[i]];

    /*
      dnums.Append (dn);
    
      elmats.SetSize (elmats.Size()+1);
      elmats.Last().AssignMemory (s, s, &mat(0,0));
    */
    /*
      if (elnr >= elmats.Size())
      {
      throw Exception ("EBE Matrix - index too high");
      }
    */
    if (elnr < elmats.Size())
      {
        dnums[elnr] = dn;
        elmats[elnr].AssignMemory (s, s, &mat(0,0));
        *testout << "dnums = " << dnums[elnr] << endl;
        *testout << "elmats[i] = " << elmats[elnr] << endl;
      }
    /*
    else
      cout << "elnr too high, elnr = " << elnr << ", size = " << elmats.Size()
           << ", dnums = " << dn << endl;
    */
  }


  template <class SCAL>
  BaseBlockJacobiPrecond * ElementByElementMatrix<SCAL> :: 
  CreateBlockJacobiPrecond (Table<int> & blocks,
                            const BaseVector * constraint, int * paralleloptions) const
  { 
    return 0;//new helmholtz_exp_cpp::BlockJacobiPrecond_ElByEl<SCAL> (*this, blocks );
  }

  template class ElementByElementMatrix<double>;
  template class ElementByElementMatrix<Complex>;

  template <class SCAL>
  HO_BilinearForm<SCAL> :: HO_BilinearForm (const FESpace & afespace, const string & aname,
                                            const Flags & flags)
    : S_BilinearForm<SCAL> (afespace, aname, flags)
  { ; }

  template <class SCAL>
  HO_BilinearForm<SCAL> :: ~HO_BilinearForm ()
  { ; }



  
  template <class SCAL>
  void HO_BilinearForm<SCAL> :: AllocateMatrix ()
  {
    cout << "alloc matrix" << endl;
    cout << "symmetric = " << this->symmetric << endl;

    const FESpace & fespace = this->fespace;

    this->mats.Append (new ElementByElementMatrix<SCAL> (fespace.GetNDof(), this->ma.GetNE()+this->ma.GetNSE()));


    cout << "generate inexact schur matrix" << endl;
    // generate inexact schur matrix

    MatrixGraph * graph = 0;

    if (this->ma.GetDimension() == 3)
      {
        int ne = this->ma.GetNE();
        int nfa = this->ma.GetNFaces();
        Array<int> vnums, ednums, elnums, dnums;
        Array<int> cnt(nfa+ne);
        cnt = 0;
	
        for (int i = 0; i < nfa; i++)
          {
            this->ma.GetFacePNums (i, vnums);

            this->ma.GetFaceEdges (i, ednums);
            this->ma.GetFaceElements (i, elnums);
	    
            /*
            // only for shells
            if ( this->ma.GetElType(elnums[0]) == ET_PRISM 
            && this->ma.GetElType(elnums[1]) == ET_PRISM )
            if (vnums.Size() == 3) continue;
            */

            for (int j = 0; j < vnums.Size(); j++)
              {
                fespace.GetVertexDofNrs (vnums[j], dnums);
                for (int k = 0; k < dnums.Size(); k++)
                  if (dnums[k] != -1) cnt[i]++;
              }

            for (int j = 0; j < ednums.Size(); j++)
              {
                fespace.GetEdgeDofNrs (ednums[j], dnums);
                for (int k = 0; k < dnums.Size(); k++)
                  if (dnums[k] != -1) cnt[i]++;
              }

            fespace.GetFaceDofNrs (i, dnums);
            for (int k = 0; k < dnums.Size(); k++)
              if (dnums[k] != -1) cnt[i]++;


            for (int j = 0; j < elnums.Size(); j++)
              {
                fespace.GetWireBasketDofNrs (elnums[j], dnums);
                for (int k = 0; k < dnums.Size(); k++)
                  if (dnums[k] != -1) cnt[i]++;
              }
          }

        /*
          for (int i = 0; i < ne; i++)
          {
          fespace.GetExternalDofNrs (i, dnums);
          for (int j = 0; j < dnums.Size(); j++)
          if (dnums[j] != -1)
          cnt[nfa+i]++;
          }
        */

        Table<int> fa2dof(cnt);

        for (int i = 0; i < nfa; i++)
          {
            this->ma.GetFacePNums (i, vnums);

            this->ma.GetFaceElements (i, elnums);

            /*
            // only for shells
            if ( this->ma.GetElType(elnums[0]) == ET_PRISM 
            && this->ma.GetElType(elnums[1]) == ET_PRISM )
            if (vnums.Size() == 3) continue;
            */

            this->ma.GetFaceEdges (i, ednums);
            int ii = 0;
            for (int j = 0; j < vnums.Size(); j++)
              {
                fespace.GetVertexDofNrs (vnums[j], dnums);
                for (int k = 0; k < dnums.Size(); k++)
                  if (dnums[k] != -1) fa2dof[i][ii++] = dnums[k];
              }
            for (int j = 0; j < ednums.Size(); j++)
              {
                fespace.GetEdgeDofNrs (ednums[j], dnums);
                for (int k = 0; k < dnums.Size(); k++)
                  if (dnums[k] != -1) fa2dof[i][ii++] = dnums[k];
              }

            fespace.GetFaceDofNrs (i, dnums);

            for (int k = 0; k < dnums.Size(); k++)
              if (dnums[k] != -1) fa2dof[i][ii++] = dnums[k];


            for (int j = 0; j < elnums.Size(); j++)
              {
                fespace.GetWireBasketDofNrs (elnums[j], dnums);
                for (int k = 0; k < dnums.Size(); k++)
                  if (dnums[k] != -1) fa2dof[i][ii++] = dnums[k];
              }
          }

        cout << "has graph table" << endl;
        // 	(*testout) << "face graph table: " << endl << fa2dof << endl;
        /*    
              for (int i = 0; i < ne; i++)
              {
              fespace.GetExternalDofNrs (i, dnums);
              int ii = 0;
              for (int j = 0; j < dnums.Size(); j++)
              if (dnums[j] != -1)
              fa2dof[nfa+i][ii++] = dnums[j];
              }
        */
        // (*testout) << "fa2dof = " << endl << fa2dof << endl;
    

        graph = new MatrixGraph (fespace.GetNDof(), fa2dof, this->symmetric);
      }
    
    else
      
      {
        // int nel = this->ma.GetNE();
        int ned = this->ma.GetNEdges();
        // int nsel = this->ma.GetNSE();
        Array<int> vnums, ednums, elnums, dnums;
        Array<int> cnt(2*ned);//+nsel);
        cnt = 0;

        for (int i = 0; i < ned; i++)
          {
            int vi[2];
            this->ma.GetEdgePNums (i, vi[0], vi[1]);

            for (int j = 0; j < 2; j++)
              {
                fespace.GetVertexDofNrs (vi[j], dnums);
                for (int k = 0; k < dnums.Size(); k++)
                  if (dnums[k] != -1) cnt[i]++;
              }

            fespace.GetEdgeDofNrs (i, dnums);
            for (int k = 0; k < dnums.Size(); k++)
              if (dnums[k] != -1) cnt[i]++;

            this->ma.GetEdgeElements (i, elnums);
            for (int j = 0; j < elnums.Size(); j++)
              {
                fespace.GetWireBasketDofNrs (elnums[j], dnums);
                for (int k = 0; k < dnums.Size(); k++)
                  if (dnums[k] != -1) cnt[i]++;
              }
          }


        Table<int> fa2dof(cnt);

        for (int i = 0; i < ned; i++)
          {
            int ii = 0;
            int vi[2];
            this->ma.GetEdgePNums (i, vi[0], vi[1]);

            for (int j = 0; j < 2; j++)
              {
                fespace.GetVertexDofNrs (vi[j], dnums);
                for (int k = 0; k < dnums.Size(); k++)
                  if (dnums[k] != -1)  fa2dof[i][ii++] = dnums[k];
              }

            fespace.GetEdgeDofNrs (i, dnums);
            for (int k = 0; k < dnums.Size(); k++)
              if (dnums[k] != -1) fa2dof[i][ii++] = dnums[k];

            this->ma.GetEdgeElements (i, elnums);
            for (int j = 0; j < elnums.Size(); j++)
              {
                fespace.GetWireBasketDofNrs (elnums[j], dnums);
                for (int k = 0; k < dnums.Size(); k++)
                  if (dnums[k] != -1) fa2dof[i][ii++] = dnums[k];
              }
          }


        cout << "has graph table" << endl;
        // (*testout) << "face graph table: " << endl << fa2dof << endl;
        /*    
              for (int i = 0; i < ne; i++)
              {
              fespace.GetExternalDofNrs (i, dnums);
              int ii = 0;
              for (int j = 0; j < dnums.Size(); j++)
              if (dnums[j] != -1)
              fa2dof[nfa+i][ii++] = dnums[j];
              }
        */
        // (*testout) << "fa2dof = " << endl << fa2dof << endl;
    

        graph = new MatrixGraph (fespace.GetNDof(), fa2dof, this->symmetric);
      }


    if (this->symmetric)
      inexact_schur = new SparseMatrixSymmetric<SCAL> (*graph, 1);
    else
      inexact_schur = new SparseMatrix<SCAL> (*graph, 1);

    delete graph;

    inexact_schur -> AsVector() = 0.0;

    //     (*testout) << "matrix = " << endl << *inexact_schur << endl;
  }


  template<class SCAL>
  BaseVector * HO_BilinearForm<SCAL> :: CreateVector() const
  {
    return new VVector<SCAL> (this->fespace.GetNDof());
  }

  template<class SCAL>
  void HO_BilinearForm<SCAL> :: AddElementMatrix (const Array<int> & dnums1,
                                                  const Array<int> & dnums2,
                                                  const FlatMatrix<SCAL> & elmat,
                                                  bool inner_element, int elnr,
                                                  LocalHeap & lh)
  {
    {
      /*
        (*testout) << "inner_element = " << inner_element << endl;
        (*testout) << "elnr = " << elnr << endl;
        (*testout) << "elmat = " << endl << elmat << endl;
        (*testout) << "dnums1 = " << endl << dnums1 << endl;
      */
      
      int nr = elnr;
      if (!inner_element) nr += this->ma.GetNE();
      dynamic_cast<ElementByElementMatrix<SCAL>&> (this->GetMatrix()) . AddElementMatrix (nr, dnums1, dnums2, elmat);
    
      
      return;
      // rest needed for Wirebasket
      


      if (inner_element)
        {
          static int timer = NgProfiler::CreateTimer ("Wirebasket matrix processing");
          NgProfiler::RegionTimer reg (timer);

          // build Schur complements for facets

        

          Array<int> usedi, unusedi, wirebasketdofs;
        
#ifdef WB_EXTRA
          this->fespace.GetWireBasketDofNrs (elnr, wirebasketdofs);
          
	  
          for (int j = 0; j < dnums1.Size(); j++)
            if (dnums1[j] != -1)
              {
                bool has = wirebasketdofs.Contains(dnums1[j]);
                
                if (has)  usedi.Append (j);
                if (!has) unusedi.Append (j);
              }
          
          int su = usedi.Size();
          int sunu = unusedi.Size();
          
          FlatMatrix<SCAL> a(su, su, lh);
          FlatMatrix<SCAL> b(su, sunu, lh);
          FlatMatrix<SCAL> c(su, sunu, lh);
          FlatMatrix<SCAL> idc(su, sunu, lh);
          FlatMatrix<SCAL> d(sunu, sunu, lh);
          
          for (int k = 0; k < su; k++)
            for (int l = 0; l < su; l++)
              a(k,l) = elmat(usedi[k], usedi[l]);
          
          for (int k = 0; k < su; k++)
            for (int l = 0; l < sunu; l++)
              {
                b(k,l) = elmat(usedi[k], unusedi[l]);
                c(k,l) = elmat(unusedi[l], usedi[k]);
              }
          
          for (int k = 0; k < sunu; k++)
            for (int l = 0; l < sunu; l++)
              d(k,l) = elmat(unusedi[k], unusedi[l]);
          
#ifdef LAPACK
          if ( sunu > 0 )
            {
              LapackInverse (d);
              LapackMultABt (c, d, idc);
              LapackMultAddABt (b, idc, -1, a);
            }
#else
          FlatMatrix<SCAL> invd(sunu, sunu, lh);
          CalcInverse (d, invd);
          d = invd;
          idc = c * Trans (invd);
          a -= b * Trans (idc);
#endif


#pragma omp critical (addinexact_schur)
          if (this->symmetric)
            {
              for (int k = 0; k < usedi.Size(); k++)
                for (int l = 0; l < usedi.Size(); l++)
                  if (dnums1[usedi[k]] >= dnums1[usedi[l]])
                    (*inexact_schur)(dnums1[usedi[k]], dnums1[usedi[l]]) += a(k,l);
            }
          else
            {
              for (int k = 0; k < usedi.Size(); k++)
                for (int l = 0; l < usedi.Size(); l++)
                  (*inexact_schur)(dnums1[usedi[k]], dnums1[usedi[l]]) += a(k,l);
            }
#endif

          if (this->ma.GetDimension() == 3)
            {

              Array<int> useddofs;
              Array<int> vnums, ednums, fanums, dnums;	

              this->ma.GetElPNums (elnr, vnums);
              this->ma.GetElEdges (elnr, ednums);
              this->ma.GetElFaces (elnr, fanums);

              for (int i = 0; i < fanums.Size(); i++)
                {
                  useddofs.SetSize (0);
                  usedi.SetSize (0);
                  unusedi.SetSize (0);

                  this->ma.GetFacePNums (fanums[i], vnums);
		
                  /*
                  // only for shells
                  if ( this->ma.GetElType(elnr) == ET_PRISM )
                  if (vnums.Size() == 3) continue;
                  */

                  // this->ma.GetElPNums (elnr, vnums);   // all vertices

                  this->fespace.GetWireBasketDofNrs (elnr, useddofs);

                  this->ma.GetFaceEdges (fanums[i], ednums);
	    
                  for (int j = 0; j < vnums.Size(); j++)
                    {
                      this->fespace.GetVertexDofNrs (vnums[j], dnums);
                      for (int k = 0; k < dnums.Size(); k++)
                        if (dnums[k] != -1) useddofs.Append (dnums[k]);
                    }
                  for (int j = 0; j < ednums.Size(); j++)
                    {
                      this->fespace.GetEdgeDofNrs (ednums[j], dnums);
                      for (int k = 0; k < dnums.Size(); k++)
                        if (dnums[k] != -1) useddofs.Append (dnums[k]);
                    }
                  this->fespace.GetFaceDofNrs (fanums[i], dnums);
                  for (int k = 0; k < dnums.Size(); k++)
                    if (dnums[k] != -1) useddofs.Append (dnums[k]);


                  for (int j = 0; j < dnums1.Size(); j++)
                    if (dnums1[j] != -1)
                      {
                        bool has = useddofs.Contains (dnums1[j]);
                        /*
                          bool wb = wirebasketdofs.Contains (dnums1[j]);
                          if (has && !wb) usedi.Append (j);
                          if (!has && !wb) unusedi.Append (j);
                        */
                        if (has) usedi.Append (j);
                        if (!has) unusedi.Append (j);
                      }

                  int su = usedi.Size();
                  int sunu = unusedi.Size();


                  FlatMatrix<SCAL> a(su, su, lh);
                  FlatMatrix<SCAL> b(su, sunu, lh);
                  FlatMatrix<SCAL> c(su, sunu, lh);
                  FlatMatrix<SCAL> d(sunu, sunu, lh);
			    
                  for (int k = 0; k < su; k++)
                    for (int l = 0; l < su; l++)
                      a(k,l) = elmat(usedi[k], usedi[l]);
			    
                  for (int k = 0; k < su; k++)
                    for (int l = 0; l < sunu; l++)
                      {
                        b(k,l) = elmat(usedi[k], unusedi[l]);
                        c(k,l) = elmat(unusedi[l], usedi[k]);
                      }
	    
                  for (int k = 0; k < sunu; k++)
                    for (int l = 0; l < sunu; l++)
                      d(k,l) = elmat(unusedi[k], unusedi[l]);

#ifdef LAPACK
                  if ( sunu > 0 )
                    {
                      LapackAInvBt (d, b);
                      LapackMultAddABt (b, c, -1, a);
                    }
#else
                  FlatMatrix<SCAL> invd(sunu, sunu, lh);
                  FlatMatrix<SCAL> idc(su, sunu, lh);
                  CalcInverse (d, invd);
                  d = invd;
                  idc = c * Trans (invd);
                  a -= b * Trans (idc);
#endif
	
#pragma omp critical (addinexact_schur)
                  {
                    if (this->symmetric)
                      {
                        for (int k = 0; k < usedi.Size(); k++)
                          for (int l = 0; l < usedi.Size(); l++)
                            if (dnums1[usedi[k]] >= dnums1[usedi[l]])
                              (*inexact_schur)(dnums1[usedi[k]], dnums1[usedi[l]]) += a(k,l);
                      }
                    else
                      {
                        for (int k = 0; k < usedi.Size(); k++)
                          for (int l = 0; l < usedi.Size(); l++)
                            (*inexact_schur)(dnums1[usedi[k]], dnums1[usedi[l]]) += a(k,l);
                      }
                  }
                }
            }
          else
            {
              Array<int> useddofs;
              Array<int> dnums, ednums;

              this->ma.GetElEdges (elnr, ednums);
              Matrix<SCAL> helmat(elmat.Height());
              helmat = elmat;

                
              for (int i = 0; i < ednums.Size(); i++)
                {
                  useddofs.SetSize (0);
                  usedi.SetSize (0);
                  unusedi.SetSize (0);

                  this->fespace.GetWireBasketDofNrs (elnr, useddofs);
                    
                  int vi[2];
                  this->ma.GetEdgePNums (ednums[i], vi[0], vi[1]);

                  for (int j = 0; j < 2; j++)
                    {
                      this->fespace.GetVertexDofNrs (vi[j], dnums);
                      for (int k = 0; k < dnums.Size(); k++)
                        if (dnums[k] != -1) useddofs.Append (dnums[k]);
                    }
                    
                  this->fespace.GetEdgeDofNrs (ednums[i], dnums);
                  for (int k = 0; k < dnums.Size(); k++)
                    if (dnums[k] != -1) useddofs.Append (dnums[k]);
                    
                  for (int j = 0; j < dnums1.Size(); j++)
                    if (dnums1[j] != -1)
                      {
                        bool has = useddofs.Contains(dnums1[j]);
                        /*
                          bool wb = wirebasketdofs.Contains(dnums1[j]);
                          if (has && !wb) usedi.Append (j);
                          if (!has && !wb) unusedi.Append (j);
                        */

                        if (has) usedi.Append (j);
                        if (!has) unusedi.Append (j);
                      }
                    
                  int su = usedi.Size();
                  int sunu = unusedi.Size();
                    
                  FlatMatrix<SCAL> a(su, su, lh);
                  FlatMatrix<SCAL> b(su, sunu, lh);
                  FlatMatrix<SCAL> c(su, sunu, lh);
                  FlatMatrix<SCAL> idc(su, sunu, lh);
                  FlatMatrix<SCAL> d(sunu, sunu, lh);
                    
                  for (int k = 0; k < su; k++)
                    for (int l = 0; l < su; l++)
                      a(k,l) = helmat(usedi[k], usedi[l]);
                    
                  for (int k = 0; k < su; k++)
                    for (int l = 0; l < sunu; l++)
                      {
                        b(k,l) = helmat(usedi[k], unusedi[l]);
                        c(k,l) = helmat(unusedi[l], usedi[k]);
                      }
                    
                  for (int k = 0; k < sunu; k++)
                    for (int l = 0; l < sunu; l++)
                      d(k,l) = helmat(unusedi[k], unusedi[l]);
                    

                  /*
                    (*testout) << "usedi = " << endl << usedi << endl;
                    (*testout) << "unusedi = " << endl << unusedi << endl;
                    (*testout) << "a = " << endl << a << endl;
                    (*testout) << "b = " << endl << b << endl;
                    (*testout) << "c = " << endl << c << endl;
                    (*testout) << "d = " << endl << d << endl;
                  */

                    
#ifdef LAPACK
                  if ( sunu > 0 )
                    {
                      LapackInverse (d);
                      LapackMultABt (c, d, idc);
                      LapackMultAddABt (b, idc, -1, a);
                    }
#else
                  FlatMatrix<SCAL> invd(sunu, sunu, lh);
                  CalcInverse (d, invd);
                  d = invd;
                  idc = c * Trans (invd);
                  a -= b * Trans (idc);
#endif

                  // *testout << "schur = " << endl << a << endl;

                  /*
                    for (int k = 0; k < su; k++)
                    for (int l = 0; l < su; l++)
                    helmat(usedi[k], usedi[l]) -= 0.99999 * a(k,l);
                  */


#pragma omp critical (addinexact_schur)
                  if (this->symmetric)
                    {
                      for (int k = 0; k < usedi.Size(); k++)
                        for (int l = 0; l < usedi.Size(); l++)
                          if (dnums1[usedi[k]] >= dnums1[usedi[l]])
                            (*inexact_schur)(dnums1[usedi[k]], dnums1[usedi[l]]) += a(k,l);
                    }
                  else
                    {
                      for (int k = 0; k < usedi.Size(); k++)
                        for (int l = 0; l < usedi.Size(); l++)
                          (*inexact_schur)(dnums1[usedi[k]], dnums1[usedi[l]]) += a(k,l);
                    }
                }
              // *testout << "helmat = " << endl << helmat << endl;
            }
        }
      else
        {

#pragma omp critical (addinexact_schur)
          {
	  
            if (this -> symmetric)
              {
                for (int i = 0; i < dnums1.Size(); i++)
                  for (int j = 0; j < dnums2.Size(); j++)
                    if (dnums1[i] != -1 && dnums2[j] != -1)
                      if (dnums1[i] >= dnums1[j])
                        (*inexact_schur)(dnums1[i], dnums2[j]) += elmat(i, j);
              }
            else
              {
                for (int i = 0; i < dnums1.Size(); i++)
                  for (int j = 0; j < dnums2.Size(); j++)
                    if (dnums1[i] != -1 && dnums2[j] != -1)
                      (*inexact_schur)(dnums1[i], dnums2[j]) += elmat(i, j);
              }
          }
        }
    }
  }





  template class HO_BilinearForm<double>;
  template class HO_BilinearForm<Complex>;





  template <class SCAL>
  WireBasketPreconditioner<SCAL> ::
  WireBasketPreconditioner (const PDE * pde, const Flags & aflags, const string aname)
    : Preconditioner (pde, aflags, aname)
  {
    cout << "\ncreate wire-basket precond" << endl;
    bfa = dynamic_cast<const HO_BilinearForm<SCAL>*>(pde->GetBilinearForm (aflags.GetStringFlag ("bilinearform", NULL)));
    inversetype = flags.GetStringFlag("inverse", "sparsecholesky");
    smoothingtype = int(flags.GetNumFlag("blocktype", -1));
  }


  template <class SCAL>
  void WireBasketPreconditioner<SCAL> ::
  Update ()
  {
    cout << "update wirebasket" << endl;

    const SparseMatrix<SCAL> & mat = bfa -> InexactSchur();

    //      const SparseMatrix<SCAL> & mat = 
    //	dynamic_cast<const SparseMatrix<SCAL>& > (bfa -> GetMatrix());

    //       (*testout) << "type = " << typeid(mat).name() << endl;
    //       (*testout) << "mat = " << mat << endl;
      
    mat.SetInverseType(inversetype);
    cout << "inversetype = " << inversetype << endl;
    if ( smoothingtype == -1 )
      {
        pre = mat.InverseMatrix(); // const_cast<SparseMatrix<SCAL>*> (&mat); // 
      }
    else
      {
        Array<int> cnt(ma.GetNE());
        // const ElementByElementMatrix<SCAL> & elbyelmat =
        // dynamic_cast<const ElementByElementMatrix<SCAL>&> (bfa -> GetMatrix() );
        // 	  const helmholtz_exp_cpp::HybridHelmholtzFESpace & hhspace = 
        // 	    dynamic_cast<const helmholtz_exp_cpp::HybridHelmholtzFESpace & > 
        // 	    (bfa -> GetFESpace() );
	  
        Table<int> * blocktable = bfa->GetFESpace().CreateSmoothingBlocks(smoothingtype);
	  
        pre = mat.  CreateBlockJacobiPrecond (*blocktable);
      }
    if (test) Test();
  }


  template class WireBasketPreconditioner<double>;
  template class WireBasketPreconditioner<Complex>;



  class BDDCMatrix : public BaseMatrix
  {
    const HO_BilinearForm<double> & bfa;
    Array<int> restrict;
    Array<int> multiple;
    BaseMatrix * inv;

  public:
    BDDCMatrix (const HO_BilinearForm<double> & abfa)
      : bfa(abfa) 
    {
      LocalHeap lh(10000000);
      const FESpace & fes = bfa.GetFESpace();
      const MeshAccess & ma = fes.GetMeshAccess();

      int ne = ma.GetNE();
      Array<int> cnt(ne);

      Array<int> wbdofs(fes.GetNDof()), lwbdofs, dnums;
      wbdofs = 0;

      for (int i = 0; i < ne; i++)
        {
          fes.GetExternalDofNrs (i, dnums);
          cnt[i] = dnums.Size();

          fes.GetWireBasketDofNrs (i, lwbdofs);
          for (int j = 0; j < lwbdofs.Size(); j++)
            wbdofs[lwbdofs[j]] = 1;
        }

      Table<int> dcdofs(cnt);   // discontinuous dofs

      

      int firstdcdof = 0;
      for (int i = 0; i < wbdofs.Size(); i++)
        if (wbdofs[i]) firstdcdof = i+1;

      for (int i = 0; i < ne; i++)
        {
          fes.GetExternalDofNrs (i, dnums);

          for (int j = 0; j < dnums.Size(); j++)
            {
              if (dnums[j] == -1) continue;
              if (wbdofs[dnums[j]]) continue;
              dnums[j] = firstdcdof;
              firstdcdof++;
            }

          for (int j = 0; j < dnums.Size(); j++)
            dcdofs[i][j] = dnums[j];
        }

      restrict.SetSize(firstdcdof);
      restrict = -1;
      for (int i = 0; i < ne; i++)
        {
          fes.GetExternalDofNrs (i, dnums);

          for (int j = 0; j < dnums.Size(); j++)
            {
              if (dnums[j] != -1)
                restrict[dcdofs[i][j]] = dnums[j];
            }
        }

      multiple.SetSize (fes.GetNDof());
      multiple = 0;
      for (int i = 0; i < restrict.Size(); i++)
        if (restrict[i] != -1)
          multiple[restrict[i]]++;

      MatrixGraph graph(firstdcdof, dcdofs, 1);
      SparseMatrixSymmetric<double> dcmat(graph, 1);
      dcmat.SetInverseType ("sparsecholesky");
      
      for (int i = 0; i < ne; i++)
        {
          FlatMatrix<> elmat = 
            dynamic_cast<const ElementByElementMatrix<double>&> (bfa.GetMatrix()) . GetElementMatrix (i);

          FlatArray<int> dofs = dcdofs[i];
          for (int k = 0; k < dofs.Size(); k++)
            for (int l = 0; l < dofs.Size(); l++)
              if (dofs[k] != -1 && dofs[l] != -1 && dofs[k] >= dofs[l])
                dcmat(dofs[k], dofs[l]) += elmat(k,l);
        }

      inv = dcmat.InverseMatrix();
    }


    virtual void MultAdd (double s, const BaseVector & x, BaseVector & y) const
    {
      FlatVector<> fx = x.FVDouble();
      FlatVector<> fy = y.FVDouble();

      VVector<> lx(restrict.Size());
      VVector<> ly(restrict.Size());

      for (int i = 0; i < restrict.Size(); i++)
        if (restrict[i] != -1)
          lx(i) = fx(restrict[i]) / multiple[restrict[i]];

      ly = (*inv) * lx;
            
      fy = 0.0;
      for (int i = 0; i < restrict.Size(); i++)
        if (restrict[i] != -1)
          fy(restrict[i]) += ly(i) / multiple[restrict[i]];
    }

  };











  class BDDCMatrixRefElement : public BaseMatrix
  {
    const S_BilinearForm<double> & bfa;
    Array<int> restrict;
    Array<int> multiple;
    BaseMatrix * inv;
    
    Table<int> *wbdofs, *dcdofs;
    Table<int> *globwbdofs, *globdcdofs;

    Array<int> elclassnr;
    Array<Matrix<>*> invdc_ref, extwb_ref, schurwb_ref;
    int ne;
    
  public:
    BDDCMatrixRefElement (const S_BilinearForm<double> & abfa)
      : bfa(abfa) 
    {
      LocalHeap lh(10000000);
      const FESpace & fes = bfa.GetFESpace();
      const MeshAccess & ma = fes.GetMeshAccess();

      ne = ma.GetNE();
      Array<int> cnt(ne), cntwb(ne), cntdc(ne);

      Array<int> gwbdofs(fes.GetNDof()), lwbdofs, dnums, dcdnums, vnums;
      gwbdofs = 0;

      for (int i = 0; i < ne; i++)
        {
          fes.GetExternalDofNrs (i, dnums);
          fes.GetWireBasketDofNrs (i, lwbdofs);

          cnt[i] = dnums.Size();
          cntwb[i] = lwbdofs.Size();
          cntdc[i] = dnums.Size()-lwbdofs.Size();
          for (int j = 0; j < lwbdofs.Size(); j++)
            gwbdofs[lwbdofs[j]] = 1;
        }

      Table<int> dcdofnrs(cnt);   // discontinuous dofs
      wbdofs = new Table<int>(cntwb);
      dcdofs = new Table<int>(cntdc);
      globwbdofs = new Table<int>(cntwb);
      globdcdofs = new Table<int>(cntdc);
      

      int totdofs  = fes.GetNDof();
      for (int i = 0; i < ne; i++)
        {
          fes.GetExternalDofNrs (i, dnums);
          dcdnums.SetSize (dnums.Size());

          for (int j = 0; j < dnums.Size(); j++)
            {
              dcdnums[j] = dnums[j];
              if (dcdnums[j] == -1) continue;
              if (gwbdofs[dcdnums[j]]) continue;
              dcdnums[j] = totdofs;
              totdofs++;
            }


          int wbi = 0, dci = 0;
          for (int j = 0; j < dnums.Size(); j++)
            {
              if (dnums[j] == -1) continue;
              if (gwbdofs[dnums[j]])
                {
                  (*wbdofs)[i][wbi] = j;
                  (*globwbdofs)[i][wbi] = dnums[j];
                  wbi++;
                }
              else
                {
                  (*dcdofs)[i][dci] = j;
                  (*globdcdofs)[i][dci] = dcdnums[j];
                  dci++;
                }
            }
          
          for (int j = 0; j < dnums.Size(); j++)
            dcdofnrs[i][j] = dcdnums[j];
        }

      int firstdcdof = fes.GetNDof();      
      
      // MatrixGraph graph(firstdcdof, dcdofnrs, 1);
      MatrixGraph graph(firstdcdof, *globwbdofs, 1);
      SparseMatrixSymmetric<double> dcmat(graph, 1);
      dcmat.SetInverseType ("sparsecholesky");


      elclassnr.SetSize(ne);

      invdc_ref.SetSize(32);
      extwb_ref.SetSize(32);
      schurwb_ref.SetSize(32);
      invdc_ref = NULL;
      extwb_ref = NULL;
      schurwb_ref = NULL;

      for (int i = 0; i < ne; i++)
        {
          HeapReset hr(lh);
          ma.GetElVertices(i, vnums);

          int classnr = 0;

          int sort[4] = { 0, 1, 2, 3 };
          if (vnums.Size() == 4)
            {
              if (vnums[sort[0]] > vnums[sort[1]]) { Swap (sort[0], sort[1]); classnr += 1; }
              if (vnums[sort[2]] > vnums[sort[3]]) { Swap (sort[2], sort[3]); classnr += 2; }
              if (vnums[sort[0]] > vnums[sort[2]]) { Swap (sort[0], sort[2]); classnr += 4; }
              if (vnums[sort[1]] > vnums[sort[3]]) { Swap (sort[1], sort[3]); classnr += 8; }
              if (vnums[sort[1]] > vnums[sort[2]]) { Swap (sort[1], sort[2]); classnr += 16; }
            }
          else
            {
              if (vnums[sort[0]] > vnums[sort[1]]) { Swap (sort[0], sort[1]); classnr += 1; }
              if (vnums[sort[1]] > vnums[sort[2]]) { Swap (sort[1], sort[2]); classnr += 2; }
              if (vnums[sort[0]] > vnums[sort[1]]) { Swap (sort[0], sort[1]); classnr += 4; }
            }
          elclassnr[i] = classnr;
          if (invdc_ref[classnr]) continue;

          cout << "assemble class " << classnr << endl;

          fes.GetExternalDofNrs (i, dnums);

          Matrix<> elmat(dnums.Size());
          Matrix<> partelmat(dnums.Size());
          elmat = 0.0;

          ElementTransformation eltrans;
          ma.GetElementTransformation (i, eltrans, lh);
          const FiniteElement & fel = fes.GetFE(i, lh);

          for (int j = 0; j < bfa.NumIntegrators(); j++)
            {
              bfa.GetIntegrator(j)->AssembleElementMatrix (fel, eltrans, partelmat, lh);
              elmat += partelmat;
            }


          int nwb = (*wbdofs)[i].Size();
          int ndc = (*dcdofs)[i].Size();

          Matrix<> matwbwb(nwb);
          Matrix<> matdcwb(ndc, nwb);
          Matrix<> extdcwb(ndc, nwb);
          Matrix<> matdcdc(ndc);

          for (int j = 0; j < nwb; j++)
            for (int k = 0; k < nwb; k++)
              matwbwb(j,k) = elmat((*wbdofs)[i][j], (*wbdofs)[i][k]);
          for (int j = 0; j < ndc; j++)
            for (int k = 0; k < nwb; k++)
              matdcwb(j,k) = elmat((*dcdofs)[i][j], (*wbdofs)[i][k]);
          for (int j = 0; j < ndc; j++)
            for (int k = 0; k < ndc; k++)
              matdcdc(j,k) = elmat((*dcdofs)[i][j], (*dcdofs)[i][k]);

          LapackInverse (matdcdc);
          extdcwb = matdcdc * matdcwb;
          matwbwb -= Trans (matdcwb) * extdcwb;

          if (invdc_ref[classnr] == NULL)
            {
              invdc_ref[classnr] = new Matrix<> (ndc, ndc);
              extwb_ref[classnr] = new Matrix<> (ndc, nwb);
              schurwb_ref[classnr] = new Matrix<> (nwb, nwb);
              *(invdc_ref[classnr]) = matdcdc;
              *(extwb_ref[classnr]) = extdcwb;
              *(schurwb_ref[classnr]) = matwbwb;
            }
        }


      for (int i = 0; i < ne; i++)
        {
          FlatArray<int> dofs = (*globwbdofs)[i]; // dcdofnrs[i];
          FlatMatrix<> matwbwb = *schurwb_ref[elclassnr[i]];
          for (int k = 0; k < dofs.Size(); k++)
            for (int l = 0; l < dofs.Size(); l++)
              if (dofs[k] != -1 && dofs[l] != -1 && dofs[k] >= dofs[l])
                dcmat(dofs[k], dofs[l]) += matwbwb(k,l);
        }

      restrict.SetSize(totdofs);
      restrict = -1;
      for (int i = 0; i < ne; i++)
        {
          fes.GetExternalDofNrs (i, dnums);

          for (int j = 0; j < dnums.Size(); j++)
            {
              if (dnums[j] != -1)
                restrict[dcdofnrs[i][j]] = dnums[j];
            }
        }

      multiple.SetSize (fes.GetNDof());
      multiple = 0;
      for (int i = 0; i < restrict.Size(); i++)
        if (restrict[i] != -1)
          multiple[restrict[i]]++;


      for (int i = 0; i < firstdcdof; i++)
        if (gwbdofs[i] == 0)
          dcmat(i,i) = 1;

      inv = dcmat.InverseMatrix();
    }


    virtual void MultAdd (double s, const BaseVector & x, BaseVector & y) const
    {
      static int timer = NgProfiler::CreateTimer ("Apply BDDC preconditioner");
      static int timerrest = NgProfiler::CreateTimer ("Apply BDDC preconditioner, restrict");
      static int timerdc = NgProfiler::CreateTimer ("Apply BDDC preconditioner, decoupled");
      static int timerext = NgProfiler::CreateTimer ("Apply BDDC preconditioner, extend");
      NgProfiler::RegionTimer reg (timer);

      LocalHeap lh(100000);
      FlatVector<> fx = x.FVDouble();
      FlatVector<> fy = y.FVDouble();

      VVector<> lx(restrict.Size());
      VVector<> ly(restrict.Size());

      lx = 0.0;
      for (int i = 0; i < restrict.Size(); i++)
        if (restrict[i] != -1)
          lx(i) = fx(restrict[i]) / multiple[restrict[i]];


      // restrict
      NgProfiler::StartTimer (timerrest);
#pragma omp parallel
      {
        LocalHeap lh(100000);
#pragma omp for
      for (int i = 0; i < ne; i++)
        {
          HeapReset hr(lh);
          FlatArray<int> wbd = (*globwbdofs)[i];
          FlatArray<int> dcd = (*globdcdofs)[i];

          FlatVector<> wb(wbd.Size(), lh);
          FlatVector<> dc(dcd.Size(), lh);

          for (int j = 0; j < dcd.Size(); j++)
            dc(j) = lx(dcd[j]);
          
          // wb = Trans(*extwb[i]) * dc;
          wb = Trans(*extwb_ref[elclassnr[i]]) * dc;
          
          for (int j = 0; j < wbd.Size(); j++)
            lx(wbd[j]) -= wb(j);

          NgProfiler::AddFlops (timer, 2*wbd.Size()*dcd.Size());
          NgProfiler::AddFlops (timerrest, wbd.Size()*dcd.Size());
        }
      }
      NgProfiler::StopTimer (timerrest);

      ly = 0.0;
      ly = (*inv) * lx;


      // solve decoupled
      NgProfiler::StartTimer (timerdc);
#pragma omp parallel
      {
        LocalHeap lh(100000);
#pragma omp for
        for (int i = 0; i < ne; i++)
          {
            HeapReset hr(lh);
            FlatArray<int> dcd = (*globdcdofs)[i];
            
            FlatVector<> dc1(dcd.Size(), lh);
            FlatVector<> dc2(dcd.Size(), lh);
            
            for (int j = 0; j < dcd.Size(); j++)
              dc1(j) = lx(dcd[j]);
            
            // dc2 = Trans(*invdc[i]) * dc1;
            dc2 = (*invdc_ref[elclassnr[i]]) * dc1;
            
            for (int j = 0; j < dcd.Size(); j++)
              ly(dcd[j]) = dc2(j);
            
            NgProfiler::AddFlops (timer, sqr(dcd.Size()));
            NgProfiler::AddFlops (timerdc, sqr(dcd.Size()));
          }
      }
      NgProfiler::StopTimer (timerdc);

      // extend
      NgProfiler::StartTimer (timerext);
#pragma omp parallel
      {
        LocalHeap lh(100000);
#pragma omp for
      for (int i = 0; i < ne; i++)
        {
          HeapReset hr(lh);
          FlatArray<int> wbd = (*globwbdofs)[i];
          FlatArray<int> dcd = (*globdcdofs)[i];

          FlatVector<> wb(wbd.Size(), lh);
          FlatVector<> dc(dcd.Size(), lh);
          for (int j = 0; j < wbd.Size(); j++)
            wb(j) = ly(wbd[j]);

          // dc = (*extwb[i]) * wb;
          dc = (*extwb_ref[elclassnr[i]]) * wb;

          for (int j = 0; j < dcd.Size(); j++)
            ly(dcd[j]) -= dc(j);
          NgProfiler::AddFlops (timerext, wbd.Size()*dcd.Size());
        }
      }
      NgProfiler::StopTimer (timerext);

      fy = 0.0;
      for (int i = 0; i < restrict.Size(); i++)
        if (restrict[i] != -1)
          fy(restrict[i]) += ly(i) / multiple[restrict[i]];
    }

  };













  template <class SCAL>
  BDDCPreconditioner<SCAL> ::
  BDDCPreconditioner (const PDE * pde, const Flags & aflags, const string aname)
    : Preconditioner (pde, aflags, aname)
  {
    bfa = dynamic_cast<const S_BilinearForm<SCAL>*>(pde->GetBilinearForm (aflags.GetStringFlag ("bilinearform", NULL)));
    inversetype = flags.GetStringFlag("inverse", "sparsecholesky");
  }


  template <class SCAL>
  void BDDCPreconditioner<SCAL> ::
  Update ()
  {
    cout << "update bddc" << endl;
    // pre = new BDDCMatrix(*bfa);
    pre = new BDDCMatrixRefElement(*bfa);

    if (test) Test();
    
  }  



  template class BDDCPreconditioner<double>;
    
  namespace wirebasket_cpp
  {
   
    class Init
    { 
    public: 
      Init ();
    };
    
    Init::Init()
    {
      GetPreconditionerClasses().AddPreconditioner ("wirebasket", WireBasketPreconditioner<double>::Create);
      GetPreconditionerClasses().AddPreconditioner ("wirebasket_complex", WireBasketPreconditioner<Complex>::Create);

      GetPreconditionerClasses().AddPreconditioner ("bddc", BDDCPreconditioner<double>::Create);
    }
    
    Init init;
  }


}
