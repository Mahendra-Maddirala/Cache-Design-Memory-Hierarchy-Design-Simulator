#include <bits/stdc++.h>
#include <stdlib.h>
#include <inttypes.h>
#include <map>
#include <iostream>
#include "sim.h"

using namespace std;
#define ll uint32_t
/*  "argc" holds the number of command-line arguments.
    "argv[]" holds the arguments themselves.

    Example:
    ./sim 32 8192 4 262144 8 3 10 gcc_trace.txt
    argc = 9
    argv[0] = "./sim"
    argv[1] = "32"
    argv[2] = "8192"
    ... and so on
*/
class Cache
{
   public:
   ll block_size, l1_size, l1_assoc, l2_size, l2_assoc;
   ll pref_n, pref_m;
   ll l1_sets, l1_index_bits, l1_block_bits, l1_tag_bits;
   ll l2_sets, l2_index_bits, l2_block_bits, l2_tag_bits;
   blocks **l1_cache, **l2_cache, **prefetch;
   ll *prefetch_rank, *prefetch_valid;
   bool l1_prefetch;
   bool l2_prefetch;

   ll l1_r, l1_r_miss ,l1_r_miss_pref_hit, l1_w, l1_w_miss, l1_w_miss_pref_hit;
   ll l1_w_back, l1_pref;
   ll l2_r, l2_r_miss, l2_r_miss_pref_hit, l2_w, l2_w_miss, l2_w_miss_pref_hit;
   ll l2_w_back, l2_pref, total_memory_traffic;

   Cache(ll b, ll l1, ll l1_a, ll l2, ll l2_a, ll n, ll m){
      block_size = b;
      l1_size = l1;
      l1_assoc = l1_a;
      l2_size = l2;
      l2_assoc = l2_a;
      pref_m = m;
      pref_n = n;

      l1_r =l1_r_miss = l1_r_miss_pref_hit=l1_w=l1_w_miss=l1_w_miss_pref_hit=l1_w_back=l1_pref
    =l2_r=l2_r_miss=l2_r_miss_pref_hit=l2_w=l2_w_miss=l2_w_miss_pref_hit
   =l2_w_back=l2_pref=total_memory_traffic =0;

    l1_prefetch = l2_prefetch = 0;
      if(l1_size!=0 && l1_assoc!=0)
      {
         l1_sets = l1_size/(l1_assoc*block_size);
         l1_index_bits = log2(l1_sets);
         l1_block_bits = log2(block_size);
         l1_tag_bits = 32 - l1_index_bits - l1_block_bits;
         
         l1_cache = new blocks* [l1_sets];
         //cout<<sizeof(l1_cache)<<endl;
         for(ll i=0;i<l1_sets;i++)
            l1_cache[i] = new blocks[l1_assoc];
         for(ll i=0;i<l1_sets;i++)
         {
            for(ll j=0;j<l1_assoc;j++)
            {
               l1_cache[i][j].dirty = 0;
               l1_cache[i][j].rank = j;
               l1_cache[i][j].tag = 0;
               l1_cache[i][j].valid = 0;
            }
         }
      }

      if(l2_size!=0 && l2_assoc!=0)
      {
         l2_sets = l2_size/(l2_assoc*block_size);
         l2_index_bits = log2(l2_sets);
         l2_block_bits = log2(block_size);
         l2_tag_bits = 32 - l2_index_bits - l2_block_bits;

         l2_cache = new blocks* [l2_sets];
         for(ll i=0;i<l2_sets;i++)
            l2_cache[i] = new blocks[l2_assoc];
         
         for(ll i=0;i<l2_sets;i++)
         {
            for(ll j=0;j<l2_assoc;j++)
            {
               l2_cache[i][j].dirty = 0;
               l2_cache[i][j].rank = j;
               l2_cache[i][j].tag = 0;
               l2_cache[i][j].valid = 0;
            }
         }
      }

      

      
      if(pref_m!=0 && pref_n!=0)
      {
         
         if(l2_size==0)l1_prefetch=true;
         else l2_prefetch=true;

         prefetch_rank = new ll[pref_n];
         prefetch_valid = new ll[pref_n];
         prefetch = new blocks* [pref_n];
         for(ll i=0;i<pref_n;i++)
            prefetch[i]= new blocks[pref_m];
         for(ll i=0;i<pref_n;i++)
         {
            prefetch_rank[i] = i;
            prefetch_valid[i]=0;
            for(ll j=0;j<pref_m;j++)
            {
               prefetch[i][j].valid = 0;
               prefetch[i][j].dirty = 0;
               prefetch[i][j].rank = 0;
               prefetch[i][j].tag = 0 ;
            }
         }
      }
   }

   void prefetch_rank_update(ll j)
   {
      for(ll i=0;i<pref_n;i++)
      {
         if(prefetch_rank[i]<prefetch_rank[j])prefetch_rank[i]++;
      }
      prefetch_rank[j]=0; //CHANGE
   }

   void rank_update(blocks **l, ll index, ll assoc, ll block)
   {
      for(ll i=0;i<assoc;i++)
      {
         if (l[index][i].rank < l[index][block].rank)
         {
            l[index][i].rank++;
         }
      }
      l[index][block].rank=0;
   }

   bool l1_check(ll index,ll tag,char c)
   {
      for(ll i=0;i<l1_assoc;i++)
      {
         if(l1_cache[index][i].tag == tag && l1_cache[index][i].valid == 1)
         {
            if(c=='w')l1_cache[index][i].dirty=1;
            rank_update(l1_cache,index,l1_assoc,i);
            return true;
         }
      }
      return false;
   }

   bool l2_check(ll index,ll tag,char c)
   {
      for(ll i=0;i<l2_assoc;i++)
      {
         if(l2_cache[index][i].tag == tag && l2_cache[index][i].valid == 1)
         {
            if(c=='w')l2_cache[index][i].dirty=1;
            rank_update(l2_cache,index,l2_assoc,i);
            return true;
         }
      }
      return false;
   }
   
   ll lru_prefetch()
   {
      ll i = 0;
      ll max=0;
      for(ll j=0;j<pref_n;j++)
      {
         if(prefetch_valid[j]==0)return j;
         if(prefetch_rank[j]>=max)
         {
            max=prefetch_rank[j];
            i=j;
         }
      }
      return i;
   }

   ll lru_cache(blocks** l, ll i, ll a)
   {
      ll ans = 0;
      ll max=0;
      //bool f;
      for(ll j=0;j<a;j++)
      {
         
         //f = l[i][j].rank>max;
         if(l[i][j].valid==0)return j;
         if(l[i][j].rank>=max)
         {
            max=l[i][j].rank;
            ans=j;
         }
      }
      return ans;
   }

   void l1_read(ll &addr) 
   {
      //if(l1_r>600||l1_w>600)return;
      //cout<<l1_r<<endl;
      l1_r++;
      //ll block_no = ((1 << l1_block_bits) - 1) & addr;
      ll index =  ((1 << l1_index_bits) - 1) & (addr>>l1_block_bits);
      ll tag = addr >> (l1_block_bits + l1_index_bits);

      // //cout<<"index: "<<index<<" tag: "<<tag<<" "<<block_no<<" "<<endl;

      // for(ll i=0;i<l1_assoc;i++)
      // {
      //    //cout<<l1_cache[index][i].tag<<" ";
      // }cout<<" true ";
      // cout<<endl<<" f ";
      //cout<<"tag: "<<tag<<endl;
      //cout<<"l1_pre: "<<l1_prefetch<<endl;
      if(l1_check(index,tag,'r')==true)//if tag is in l1
      {
         
         if(l1_prefetch)// if prefetch exists
         {
            bool pref_hit=false;
            for(ll i=0;i<pref_n;i++)
            {
               if(pref_hit==true)break;
               for(ll j=0;j<pref_m;j++)
               {
                  if(pref_hit==true)break;
                  if(prefetch_valid[i]==1 && prefetch[i][j].tag==tag)//if prefetch has tag
                  {
                     pref_hit=true;
                  
                     if(j!=pref_m-1)
                     {
                        for(ll k=j+1;k<pref_m;k++)
                        {
                           prefetch[i][k-j-1].tag=prefetch[i][k].tag;
                           }
                        for(ll k=pref_m-j-1;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=prefetch[i][k-1].tag+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                        prefetch_valid[i]=1;
                     }
                     else
                     {
                        prefetch[i][0].tag=tag+1;
                        total_memory_traffic++;
                        l1_pref++;
                        for(ll k=1;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=prefetch[i][k-1].tag+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                        prefetch_valid[i]=1;
                     }
                     prefetch_rank_update(i);
                  }
               }
            }
         }// nothing to do if prefetch doesn't have tag and l1 has
      }
      
      else//if l1 doesn't have tag
      {
         //cout<<"true";
         l1_r_miss++;
         if(l1_prefetch) //if prefetch exists
         {
            bool pref_hit=false;
            for(ll i=0;i<pref_n;i++)
            {
               if(pref_hit==true)break;
               for(ll j=0;j<pref_m;j++)
               {
                  if(pref_hit==true)break;
                 // cout<<"rr"<<endl;
                 // cout<<sizeof(prefetch)<<" "<<sizeof(blocks)<<endl;
                  if(prefetch_valid[i]==1 && prefetch[i][j].tag==tag)//prefetch has tag
                  {
                     
                     l1_r_miss_pref_hit++;
                     pref_hit=true;
                  
                     if(j!=pref_m-1)
                     {
                        for(ll k=j+1;k<pref_m;k++)
                        {
                           prefetch[i][k-j-1].tag=prefetch[i][k].tag;
                        }
                        for(ll k=pref_m-j-1;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=prefetch[i][k-1].tag+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                        prefetch_valid[i]=1;
                     }
                     else
                     {
                        for(ll k=0;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=tag+k+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                        prefetch_valid[i]=1;
                     }
                     prefetch_rank_update(i);
                  }
                  
               }
            }
         
            if(pref_hit==false)//prefetch doesn't have tag
            {
               ll i=lru_prefetch();
               prefetch_valid[i]=1;
               //cout << "DEBUG: i: " <<i << "\n";
               for(ll j=0;j<pref_m;j++)
               {
                  //cout << "DEBUG: j: " <<j << "\n";
                  prefetch[i][j].tag=tag+j+1;
                  l1_pref++;
                  total_memory_traffic++;//accesing main memory as l2 doesn't exist
               }
               prefetch_rank_update(i);
            }
         }

         ll i=lru_cache(l1_cache, index, l1_assoc);
         if(l1_cache[index][i].dirty==1 && l1_cache[index][i].valid==1)
         {
            l1_w_back++;
            if(l2_size!=0)
            {
               ll evict=(i) | (index <<l1_block_bits)| (l1_cache[index][i].tag<<(l1_block_bits+l1_index_bits));
               l2_write(evict);
               l2_read(addr);
            }
            else total_memory_traffic+=2;
            //as stream buffers are assumed to be updated.
            // if(l1_prefetch)//making all stream buffers containing evicting element invalid
            // {
            //    for(ll j=0;j<pref_n;j++)
            //    {
            //       for(ll k=0;k<pref_m;k++)
            //       {
            //          if(prefetch[j][k].tag==l1_cache[index][i].tag)
            //             prefetch_valid[i]=0;
            //       }
            //    }
            //    total_memory_traffic++;//as l2 doesn't exist we are writing back to main memory
            // }
            
         }

         else//not dirty
         {
            if(l2_size!=0){l2_read(addr);}
            else total_memory_traffic++;
         }
      //cout<<addr<<endl;
         l1_cache[index][i].tag=tag;
         rank_update(l1_cache,index,l1_assoc,i);
         l1_cache[index][i].valid=1;
         l1_cache[index][i].dirty=0;
      }
   }

   void l1_write(ll addr)
   {
      
      //if(l1_r>300||l1_w>300)return;
      l1_w++;
      //ll block_no = ((1 << l1_block_bits) - 1) & addr;
      ll index =  ((1 << l1_index_bits) - 1) & (addr>>l1_block_bits);
      ll tag = addr >> (l1_block_bits + l1_index_bits);

      //cout<<a<<" "<<addr<<endl;
      if(l1_check(index,tag,'w')==true)//if tag is in l1
      {
         if(l1_prefetch)// if prefetch exists
         {
            bool pref_hit=false;
            for(ll i=0;i<pref_n;i++)
            {
               if(pref_hit==true)break;
               for(ll j=0;j<pref_m;j++)
               {
                  if(pref_hit==true)break;
                  if(prefetch_valid[i]==1 && prefetch[i][j].tag==tag)//if prefetch has tag
                  {
                     pref_hit=true;
                  
                     if(j!=pref_m-1)
                     {
                        for(ll k=j+1;k<pref_m;k++)
                        {
                           prefetch[i][k-j-1].tag=prefetch[i][k].tag;
                        }
                        for(ll k=pref_m-j-1;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=prefetch[i][k-1].tag+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                     }
                     else
                     {
                        for(ll k=0;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=tag+k+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                     }
                     prefetch_valid[i]=1;
                     prefetch_rank_update(i);
                  }
               }
            }
         }// nothing to do if prefetch doesn't have tag
      }

      else//if l1 doesn't have tag
      {
         l1_w_miss++;
         if(l1_prefetch) //if prefetch exists
         {
            bool pref_hit=false;
            for(ll i=0;i<pref_n;i++)
            {
               if(pref_hit==true)break;
               for(ll j=0;j<pref_m;j++)
               {
                  if(pref_hit==true)break;
                  if(prefetch_valid[i]==1 && prefetch[i][j].tag==tag)//prefetch has tag
                  {
                     l1_w_miss_pref_hit++;
                     pref_hit=true;
                  
                     if(j!=pref_m-1)
                     {
                        for(ll k=j+1;k<pref_m;k++)
                        {
                           prefetch[i][k-j-1].tag=prefetch[i][k].tag;
                        }
                        for(ll k=pref_m-j-1;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=prefetch[i][k-1].tag+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                        prefetch_valid[i]=1;
                     }
                     else
                     {
                        for(ll k=0;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=tag+k+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                        prefetch_valid[i]=1;
                     }
                     prefetch_rank_update(i);
                  }
               }
            }
         
            if(pref_hit ==false)//prefetch doesn't have tag
            {
               ll i=lru_prefetch();
               for(ll j=0;j<pref_m;j++)
               {
                  prefetch[i][j].tag=tag+j+1;
                  l1_pref++;
                  total_memory_traffic++;
               }
               prefetch_rank_update(i);
            }
         }

         ll i=lru_cache(l1_cache, index, l1_assoc);
         //cout<<" l "<<i<<" "<<index<<" "<<l1_assoc<<endl;
         if(l1_cache[index][i].dirty==1 && l1_cache[index][i].valid==1)
         {
            l1_w_back++;
            if(l2_size!=0)
            {
               ll evict=(i) | (index <<l1_block_bits)| (l1_cache[index][i].tag<<(l1_block_bits+l1_index_bits));
               l2_write(evict);
               l2_read(addr);
            }
            else total_memory_traffic+=2;
            //as stream buffers are assumed to be updated.
            // if(l1_prefetch)//making all stream buffers containing evicting element invalid
            // {
            //    for(ll j=0;j<pref_n;j++)
            //    {
            //       for(ll k=0;k<pref_m;k++)
            //       {
            //          if(prefetch[j][k].tag==l1_cache[index][i].tag)
            //             prefetch_valid[i]=0;
            //       }
            //    }
            //    total_memory_traffic++;//as l2 doesn't exist we are writing back to main memory
            // }
            
         }
         else 
         {
            if(l2_size!=0){l2_read(addr);}
            else total_memory_traffic++;
         }
         
         //cout<<"tag: "<<tag<<endl;
         l1_cache[index][i].tag=tag;
         l1_cache[index][i].dirty=1;
         rank_update(l1_cache,index,l1_assoc,i);
         l1_cache[index][i].valid=1;
      }

   }

   void l2_read(ll addr) 
   {
      l2_r++;
      //ll block_no = ((1 << l2_block_bits) - 1) & addr;
      ll index =  ((1 << l2_index_bits) - 1) & (addr>>l2_block_bits);
      ll tag = addr >> (l2_block_bits + l2_index_bits);
         
      
      if(l2_check(index,tag,'r')==true)//if tag is in l1
      {
         if(l2_prefetch)// if prefetch exists
         {
            bool pref_hit=false;
            for(ll i=0;i<pref_n;i++)
            {
               if(pref_hit==true)break;
               for(ll j=0;j<pref_m;j++)
               {
                  if(pref_hit==true)break;
                  if(prefetch_valid[i]==1 && prefetch[i][j].tag==tag)//if prefetch has tag
                  {
                     pref_hit=true;
                  
                     if(j!=pref_m-1)
                     {
                        for(ll k=j+1;k<pref_m;k++)
                        {
                           prefetch[i][k-j-1].tag=prefetch[i][k].tag;
                        }
                        for(ll k=pref_m-j-1;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=prefetch[i][k-1].tag+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                     }
                     else
                     {
                        for(ll k=0;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=tag+k+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                     }
                     prefetch_valid[i]=1;
                     prefetch_rank_update(i);
                  }
               }
            }
         }// nothing to do if prefetch doesn't have tag
      }

      else//if l2 doesn't have tag
      {
         
         l2_r_miss++;
         //total_memory_traffic++;
         if(l2_prefetch) //if prefetch exists
         {
            bool pref_hit=false;
            for(ll i=0;i<pref_n;i++)
            {
               if(pref_hit==true)break;
               for(ll j=0;j<pref_m;j++)
               {
                  if(pref_hit==true)break;
                  //cout<<prefetch[i][j].tag<<endl;
                  //cout<<"tag: "<<endl;
                  if(prefetch_valid[i]==1 && prefetch[i][j].tag==tag)//prefetch has tag
                  {//cout<<"here"<<endl;
                    // if(pref_hit==true)break;
                     l2_r_miss_pref_hit++;
                     pref_hit=true;
                     
                     if(j!=pref_m-1)
                     {
                        for(ll k=j+1;k<pref_m;k++)
                        {
                           prefetch[i][k-j-1].tag=prefetch[i][k].tag;
                        }
                        for(ll k=pref_m-j-1;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=prefetch[i][k-1].tag+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                     }
                     else
                     {
                        for(ll k=0;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=tag+k+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                     }
                     prefetch_valid[i]=1;
                     prefetch_rank_update(i);
                  }
                  //cout<<"here"<<endl;
               }
            }
         
            if(pref_hit ==false)//prefetch doesn't have tag
            {
               ll i=lru_prefetch();
               for(ll j=0;j<pref_m;j++)
               {
                  prefetch[i][j].tag=tag+j+1;
                  l2_pref++;
                  //cout<<prefetch[i][j].tag<<endl;
                  total_memory_traffic++;
               }
            }
         }

         ll i=lru_cache(l2_cache, index, l2_assoc);
         if(l2_cache[index][i].dirty==1)
         {
            l2_w_back++;
            total_memory_traffic+=2;
            //as stream buffers are assumed to be updated.
            // if(l2_prefetch)//making all stream buffers containing evicting element invalid
            // {
            //    for(ll j=0;j<pref_n;j++)
            //    {
            //       for(ll k=0;k<pref_m;k++)
            //       {
            //          if(prefetch[j][k].tag==l2_cache[index][i].tag)
            //             prefetch_valid[i]=0;
            //       }
            //    }
            //    total_memory_traffic++;//as l2 doesn't exist we are writing back to main memory
            // }
            
         }

         else//not dirty
         {
           total_memory_traffic++;
         }
         l2_cache[index][i].tag=tag;
         rank_update(l2_cache,index,l2_assoc,i);
         l2_cache[index][i].dirty=0;
         l2_cache[index][i].valid=1;
      }
   }

   void l2_write(ll addr)
   {
      l2_w++;
      //ll block_no = ((1 << l2_block_bits) - 1) & addr;
      ll index =  ((1 << l2_index_bits) - 1) & (addr>>l2_block_bits);
      ll tag = addr >> (l2_block_bits + l2_index_bits);

      if(l2_check(index,tag,'w')==true)//if tag is in l1
      {
         if(l2_prefetch)// if prefetch exists
         {
            bool pref_hit=false;
            for(ll i=0;i<pref_n;i++)
            {
               if(pref_hit==true)break;
               for(ll j=0;j<pref_m;j++)
               {
                  if(pref_hit==true)break;
                  if(prefetch_valid[i]==1 && prefetch[i][j].tag==tag)//if prefetch has tag
                  {
                     pref_hit=true;
                  
                     if(j!=pref_m-1)
                     {
                        for(ll k=j+1;k<pref_m;k++)
                        {
                           prefetch[i][k-j-1].tag=prefetch[i][k].tag;
                        }
                        for(ll k=pref_m-j-1;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=prefetch[i][k-1].tag+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                     }
                     else
                     {
                        for(ll k=0;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=tag+k+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                     }
                     prefetch_valid[i]=1;
                     prefetch_rank_update(i);
                  }
               }
            }
         }// nothing to do if prefetch doesn't have tag
      }

      else//if l2 doesn't have tag
      {
         l2_w_miss++;
         //total_memory_traffic++;
         if(l2_prefetch) //if prefetch exists
         {
            bool pref_hit=false;
            for(ll i=0;i<pref_n;i++)
            {
               if(pref_hit==true)break;
               for(ll j=0;j<pref_m;j++)
               {
                  if(pref_hit==true)break;
                  if(prefetch_valid[i]==1 && prefetch[i][j].tag==tag)//prefetch has tag
                  {
                     l2_w_miss_pref_hit++;
                     pref_hit=true;
                  
                     if(j!=pref_m-1)
                     {
                        for(ll k=j+1;k<pref_m;k++)
                        {
                           prefetch[i][k-j-1].tag=prefetch[i][k].tag;
                        }
                        for(ll k=pref_m-j-1;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=prefetch[i][k-1].tag+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                     }
                     else
                     {
                        for(ll k=0;k<pref_m;k++)
                        {
                           prefetch[i][k].tag=tag+k+1;
                           l2_pref++;
                           total_memory_traffic++;
                        }
                     }
                     prefetch_valid[i]=1;
                     prefetch_rank_update(i);
                  }
               }
            }
         
            if(pref_hit ==false)//prefetch doesn't have tag
            {
               ll i=lru_prefetch();
               for(ll j=0;j<pref_m;j++)
               {
                  prefetch[i][j].tag=tag+j+1;
                  l2_pref++;
                  total_memory_traffic++;
               }
               prefetch_valid[i]=1;
               prefetch_rank_update(i);
            }
         }

         ll i=lru_cache(l2_cache, index, l2_assoc);
         if(l2_cache[index][i].dirty==1)
         {
            l2_w_back++;
            //as stream buffers are assumed to be updated.
            // if(l2_prefetch)//making all stream buffers containing evicting element invalid
            // {
            //    for(ll j=0;j<pref_n;j++)
            //    {
            //       for(ll k=0;k<pref_m;k++)
            //       {
            //          if(prefetch[j][k].tag==l2_cache[index][i].tag)
            //             prefetch_valid[i]=0;
            //       }
            //    }
            // }
            total_memory_traffic++;
            total_memory_traffic++;
         }
         l2_cache[index][i].tag=tag;
         l2_cache[index][i].dirty=1;
         rank_update(l2_cache,index,l2_assoc,i);
         l2_cache[index][i].valid=1;
      }

   }
};

int main (int argc, char *argv[]) 
{
   FILE *fp;			// File pointer.
   char *trace_file;		// This variable holds the trace file name.
   cache_params_t params;	// Look at the sim.h header file for the definition of struct cache_params_t.
   char rw;			// This variable holds the request's type (read or write) obtained from the trace.
   ll addr;		// This variable holds the request's address obtained from the trace.
				// The header file <inttypes.h> above defines signed and unsigned integers of various sizes in a machine-agnostic way.  "uint32_t" is an unsigned integer of 32 bits.

   // Exit with an error if the number of command-line arguments is incorrect.
   if (argc != 9) {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      exit(EXIT_FAILURE);
   }
    
   // "atoi()" (included by <stdlib.h>) converts a string (char *) to an integer (int).
   params.BLOCKSIZE = (uint32_t) atoi(argv[1]);
   params.L1_SIZE   = (uint32_t) atoi(argv[2]);
   params.L1_ASSOC  = (uint32_t) atoi(argv[3]);
   params.L2_SIZE   = (uint32_t) atoi(argv[4]);
   params.L2_ASSOC  = (uint32_t) atoi(argv[5]);
   params.PREF_N    = (uint32_t) atoi(argv[6]);
   params.PREF_M    = (uint32_t) atoi(argv[7]);
   trace_file       = argv[8];

   // Open the trace file for reading.
   fp = fopen(trace_file, "r");
   if (fp == (FILE *) NULL) {
      // Exit with an error if file open failed.
      printf("Error: Unable to open file %s\n", trace_file);
      exit(EXIT_FAILURE);
   }
    
   // Print simulator configuration.
   printf("===== Simulator configuration =====\n");
   printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
   printf("L1_SIZE:    %u\n", params.L1_SIZE);
   printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
   printf("L2_SIZE:    %u\n", params.L2_SIZE);
   printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
   printf("PREF_N:     %u\n", params.PREF_N);
   printf("PREF_M:     %u\n", params.PREF_M);
   printf("trace_file: %s\n", trace_file);
   printf("\n");

   Cache cache(params.BLOCKSIZE, params.L1_SIZE, params.L1_ASSOC, 
            params.L2_SIZE,params.L2_ASSOC,params.PREF_N,params.PREF_M);
   // Read requests from the trace file and echo them back.
   ll i=0;
   while (fscanf(fp, "%c %x\n", &rw, &addr) == 2) {	// Stay in the loop if fscanf() successfully parsed two tokens as specified.
   i++;
   //cout<<i<<" i "<<endl;

      if (rw == 'r')
         {
            //printf("r %lx\n", addr);
            cache.l1_read(addr);
         }
         
      else if (rw == 'w')
      {
         //printf("w %x\n", addr);
         cache.l1_write(addr);
      }
         
      else {
         printf("Error: Unknown request type %c.\n", rw);
	 exit(EXIT_FAILURE);
      }
      ///////////////////////////////////////////////////////
      // Issue the request to the L1 cache instance here.
      ///////////////////////////////////////////////////////
   }
   cout<<"===== L1 contents ====="<<endl; 

   map<ll, pair <ll,ll> > l1_cache;
   if(cache.l1_size!=0)
   {
      for(ll i=0;i<cache.l1_sets;i++)
      {
         l1_cache.clear();
         for(ll j=0;j<cache.l1_assoc;j++)
         {
            if(cache.l1_cache[i][j].valid!=0)
            {
               //cout<<cache.l1_cache[i][j].rank<<cache.l1_cache[i][j].tag<<endl;
               l1_cache.insert(make_pair(cache.l1_cache[i][j].rank,make_pair(cache.l1_cache[i][j].tag,cache.l1_cache[i][j].dirty)));
            }
            //cout<<" c "<<cache.l1_cache[i][j].rank<<cache.l1_cache[i][j].tag<<endl;
         }
         cout<<"set "<<dec<<i<<":";
         map<ll, pair <ll,ll> >::iterator k;
         for(k = l1_cache.begin(); k != l1_cache.end(); k++)
         {
            cout<<" "<<hex<<(k->second).first<<" ";
            if((k->second).second==1)cout<<"D ";
            else cout<<"  ";
         }
         cout <<"\n";
         //endl;
         // for(auto k:l1_cache)
         // {
         //    cout<<" "<<hex<<k.second.first<<" ";
         //    if(k.second.second==1)cout<<"D ";
         //    else cout<<"  ";
         // }
         // endl;
      }
   }


    map<ll, pair <ll,ll> > l2_cache;
   if(cache.l2_size!=0)
   {
      cout<<endl;
      cout<<"===== L2 contents ====="<<endl;
      for(ll i=0;i<cache.l2_sets;i++)
      {
         l2_cache.clear();
         for(ll j=0;j<cache.l2_assoc;j++)
         {
            if(cache.l2_cache[i][j].valid!=0)
            {
               //cout<<cache.l2_cache[i][j].rank<<cache.l2_cache[i][j].tag<<endl;
               l2_cache.insert(make_pair(cache.l2_cache[i][j].rank,make_pair(cache.l2_cache[i][j].tag,cache.l2_cache[i][j].dirty)));
            }
            //cout<<" c "<<cache.l2_cache[i][j].rank<<cache.l2_cache[i][j].tag<<endl;
         }
         cout<<"set "<<dec<<i<<":";
         map<ll, pair <ll,ll> >::iterator k;
         for(k= l2_cache.begin(); k != l2_cache.end(); k++) {
            cout<<" "<<hex<<k->second.first<<" ";
            if(k->second.second==1)cout<<"D ";
            else cout<<"  ";
         }
         cout <<"\n";
         //std::endl;
         // for(auto k:l2_cache)
         // {
         //    cout<<" "<<hex<<k.second.first<<" ";
         //    if(k.second.second==1)cout<<"D ";
         //    else cout<<"  ";
         // }
         // endl;
      }
   }

   

   map<ll,ll> prefetch_map;
   if(cache.pref_m!=0 && cache.pref_n!=0)
   {
      cout<<endl;
      cout<<"===== Stream Buffer(s) contents ====="<<endl; 
      for(ll i=0;i<cache.pref_n;i++)
      {
          prefetch_map.clear();
         if(cache.prefetch_valid[i]!=0)
         {
            //cout << "Print Valid\n";
            for(ll j=0;j<cache.pref_m;j++)
               prefetch_map.insert(make_pair(cache.prefetch[i][j].rank,cache.prefetch[i][j].tag));
         }
         map<ll,ll>::iterator k;
         for(k=prefetch_map.begin(); k != prefetch_map.end(); k++) 
         {
            //cout<<k->first<<" "<<k->second<<endl;
            cout<<" "<<k->second<<endl;
           // prefetch_map.clear();
         }
         // for(auto k:prefetch_map)
         // {
         //    cout<<k.first<<" "<<k.second<<endl;
         // }
         // prefetch_map.clear();
      }
   }
 printf("\n");
    cout<<"===== Measurements ====="<<endl; 
   cout<<dec<<"a. L1 reads:                   "<<cache.l1_r<<endl; //number of L1 reads:
   cout<<"b. L1 read misses:             "<<cache.l1_r_miss<<endl; // number of L2 reads misses: 
   cout<<"c. L1 writes:                  "<<cache.l1_w<<endl; // number of L1 writes:
   cout<<"d. L1 write misses:            "<<cache.l1_w_miss<<endl; // number of L1 write misses
   cout<<"e. L1 miss rate:               "<<setprecision(4)<<fixed<<(double)(cache.l1_w_miss+cache.l1_r_miss)/(cache.l1_r+cache.l1_w)<<endl; //total miss rate:
   cout<<"f. L1 writebacks:              "<<cache.l1_w_back<<endl; // number of L1 writebacks
   cout<<"g. L1 prefetches:              "<<cache.l1_pref<<endl; // number of L1 prefetches
   cout<<"h. L2 reads (demand):          "<<cache.l2_r<<endl; //number of L2 reads that did not originate from L1 prefetches:
   cout<<"i. L2 read misses (demand):    "<<cache.l2_r_miss<<endl; //number of L2 read misses that did not originate from L1 prefetches:
   cout<<"j. L2 reads (prefetch):        "<<"0"<<endl; //number of L2 reads that originated from L1 prefetches:
   cout<<"k. L2 read misses (prefetch):  "<<"0"<<endl; //number of L2 read misses that originated from L1 prefetches:
   cout<<"l. L2 writes:                  "<<cache.l2_w<<endl; //number of L2 writes:
   cout<<"m. L2 write misses:            "<<cache.l2_w_miss<<endl; //number of L2 write misses:
   if (isnan((double)(cache.l2_r_miss)/(cache.l2_r)))
   {
   cout<<"n. L2 miss rate:               "<<0.0000<<endl;
   }
   else
   {
   cout<<"n. L2 miss rate:               "<<setprecision(4)<<fixed<<(double)(cache.l2_r_miss)/(cache.l2_r)<<endl; //L2 miss rate:
   }
   //l2_miss_rate = isnan((double)(cache.l2_r_miss)/(cache.l2_r)) ? 0.0000 : (double)(cache.l2_r_miss)/(cache.l2_r)
   //cout<<"n. L2 miss rate:               "<<setprecision(4)<<fixed<< <<endl;
   cout<<"o. L2 writebacks:              "<<cache.l2_w_back<<endl; //number of writebacks from L2 to memory:
   cout<<"p. L2 prefetches:              "<<cache.l2_prefetch<<endl; //number of L2 prefetches:
   cout<<"q. memory traffic:             "<<dec<<cache.total_memory_traffic<<endl; //total memory traffic:


   return(0);
}