#ifndef BASE_TL_POINTER_H
#define BASE_TL_POINTER_H

// VERY simple smart pointer class

template <class T>
class smart_ptr
{
	T *ptr;

public:
	smart_ptr(T *p = 0) : ptr(p) { }
	~smart_ptr() { delete ptr; }

	smart_ptr(smart_ptr& other) : ptr(other.release()) { }

	smart_ptr &operator =(smart_ptr& other)
	{
		reset(other.release());
		return *this;
	}

	void reset(T* p = 0)
	{
		if(ptr != p)
		{
			delete ptr;
			ptr = p;
		}
	}

	T* release()
	{
		T* tmp = ptr;
		ptr = 0;
		return tmp;
	}

	T *get() const { return ptr; }

	T &operator * () const { return *ptr; }
	T *operator -> () const { return ptr; }
};

#endif
