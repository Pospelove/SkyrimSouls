#pragma once

#include "NiTMapBase.h"
#include "NiTDefaultAllocator.h"

// 10
template <class TKEY, class TVAL>
class NiTMap : public NiTMapBase<NiTDefaultAllocator<NiTMapItem<TKEY, TVAL>>, TKEY, TVAL>
{
public:
	NiTMap(unsigned int uiHashSize = 37) : NiTMapBase(uiHashSize) {
	}
	~NiTMap() {
		RemoveAll();
	}

protected:
	// @override
  virtual NiTMapItem<TKEY, TVAL>* NewItem() override
  {
    return (NiTMapItem<TKEY, TVAL>*)m_kAllocator.Allocate();
	}
  virtual void DeleteItem(NiTMapItem<TKEY, TVAL>* item) override
  {
          item->~NiTMapItem<TKEY, TVAL>();
		m_kAllocator.Deallocate(item);
	}
};
