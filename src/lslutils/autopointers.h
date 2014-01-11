#ifndef LSL_autopointers_h_
#define LSL_autopointers_h_

#include <lslutils/type_forwards.h>

namespace LSL {

class Lockable {
public:
    int lock_count;
	Lockable():lock_count(0)
	{}

	virtual ~Lockable()
	{}

    bool Locked() const {
        return lock_count>0;
    }
    void Lock() {
        ++lock_count;
        //debug_message("Lock()");
    }
    void UnLock() {
        --lock_count;
        //debug_message("UnLock()");
    }
};

template<Lockable *T>
class StaticLocker {
public:
    inline StaticLocker() {
        T->Lock();
    }
    inline ~StaticLocker() {
        T->UnLock();
	}
};

class Locker {
    Lockable &lock;
    public:
    inline Locker(Lockable &lock_): lock(lock_) {
        lock.Lock();
    }
    inline ~Locker() {
        lock.UnLock();
	}
};

class RefcountedContainer {
public:
    Lockable ref_lock;

    RefcountedContainer() {
    	//ref_lock.Lock();
		}
    /*
    RefcountedContainer* operator new( size_t zSiz){
    	RefcountedContainer* tmp=new RefcountedContainer();
    };
    */

    virtual ~RefcountedContainer() {}

    RefcountedContainer* NewCopy() {
        return new RefcountedContainer(*this);
    }
    inline  void Reference() {
        ref_lock.Lock();
    }
    inline  void Dereference() {
        ref_lock.UnLock();
    }
    inline  bool Referenced() {
        return ref_lock.Locked();
    }
};


template <class T , bool TDestroy=true>
class RefcountedPointer {
public:
 T *data;
    RefcountedPointer():data(0) {}
    RefcountedPointer (T *p):data(p) {
        if(data)data->Reference();
    }
    RefcountedPointer(const RefcountedPointer<T,TDestroy> &p):data(p.data) {
        if(data) {
            data->Reference();
        }
        else {
           // error("RefcountedPointer copy : null");
        }
	}

    /* */
template<class U>
explicit RefcountedPointer(RefcountedPointer<U> p){

	///    std::cout<<"doing dynamic cast"<<std::endl;
	     data=dynamic_cast<T*>(p.data);
     /// std::cout<<"... done"<<std::endl;*/
        if(data) {
            data->Reference();
        }
        else {
            ///error("RefcountedPointer copy : null (cast)");
        }
	}
    /* */
    ~RefcountedPointer() {
        Dispose();
	}
    RefcountedPointer<T, TDestroy> &operator= (const RefcountedPointer &p) throw() {
        if (this != &p) {
            Dispose();
            data = p.data;
            if(data) {
                data->Reference();
            }
            else {
                ///error("RefcountedPointer operator= : bad parameter");
            }
        }
        return *this;
    }
    void Dispose() {
        if(data == NULL) {
            ///error("Something wrong with RefcountedPointer.dispose");
            return;
        }
        data->Dereference();
        if(TDestroy) {
            if(!data->Referenced()) {
                delete data;
            }
        }
        data=NULL;
    }
    T &Data() const {
        return *data;
    }
    T *Ptr() const {
        return data;
    }
    RefcountedPointer<T, TDestroy> Copy() {
        if(data) {
            RefcountedPointer<T, TDestroy> result(data->NewCopy());
            return result;
        }
        else {
            RefcountedPointer<T, TDestroy> result;
            return result;
        }
    }

    T &operator *() const {
    	return *data;
    }
    T *operator ->() const {
    	return data;
    }
    bool operator <(RefcountedPointer<T, TDestroy> p) const{
    	return data<p.data;
    }
    bool operator >(RefcountedPointer<T, TDestroy> p) const{
    	return data>p.data;
    }
    bool operator ==(RefcountedPointer<T, TDestroy> p) const{
    	return data==p.data;
    }
    bool operator !=(RefcountedPointer<T, TDestroy> p) const{
    	return !(data==p.data);
    }
    bool operator <(T *p) const {
    	return data<p;
    }
    bool operator >(T *p) const {
    	return data>p;
    }
    bool operator ==(T *p) const {
    	return data==p;
    }
    bool operator !=(T *p) const {
    	return !(data==p);
    }
    /*
    operator bool() const {
    	return data;
    }*/
    bool Ok() const{
        return data;
    }
    bool ok() const{
        return data;
    }
};

template<class T> class SingleInstance{
  T *data;
  public:
  SingleInstance():data(NULL){}
  ~SingleInstance(){delete data;}
  T *Get(){
    if(!data)data=new T;
    return data;
  }
};

template <class SRC,class DEST>
class BidirectionalConnector{
  SRC  *self_end  ;
  DEST *other_end ;
  BidirectionalConnector<DEST,SRC> *other;
  public:
  BidirectionalConnector(SRC *self_end_):
  self_end(self_end_),
  other_end(NULL),
  other(NULL)
  {
  }
  ~BidirectionalConnector(){
    Disconnect();
  }
  void Disconnect(){
    if(other){
      other->other_end=NULL;
      other->other=NULL;
      other=NULL;
      other_end=NULL;
    }
  }
  void Connect(BidirectionalConnector<DEST,SRC> *other_){
    if(other_==other)return;
    Disconnect();
    if(other_){
      other_->Disconnect();
      other=other_;
      other->other=this;
      this->other_end=other->self_end;
      other->other_end=this->self_end;
    }
  }
  SRC  *GetSelfEnd(){
    return self_end;
  }
  DEST *GetOtherEnd(){
    return other_end;
  }
};

} // namespace LSL {

/**
 * \file misc.h
 * \author Dmytry Lavrov
 * \section LICENSE
Copyright 2012 by The libSpringLobby team. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#endif //LSL_autopointers_h_
