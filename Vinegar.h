#ifndef VINEGAR
#define VINEGAR

// Include this as a header in your project

// There are multiple different things that are done here that you might not like. You can easily change the library and even post a request on github for me to edit the library.
// There are some things like some tiny functions not being marked as inline, but that's because they are only hints anyways and the compiler usually knows best. Plus, I would like the best result even if the library is edited.
// This library was designed to be easy to use, understand, learn, and to be educational.

#include<iostream>
#include<thread>
#include<vector>
#include<mutex>

// All Vinegar pointers are derived from Vin_ptr_base struct.
template <typename type>
struct Vin_ptr_base {
public:
	std::mutex pointerLock;
	void* pointer;
	void Claim(type* pointer) {
		pointerLock.lock();
		this->pointer = (void*)pointer;
		pointerLock.unlock();
	}
	void Disown() {
		Claim(nullptr);
	}
	void Free() {
		pointerLock.lock();
		delete (type*)this->pointer;
		pointerLock.unlock();
		Disown();
	}
	type* GetPointer() {
		return (type*)pointer;
	}
	Vin_ptr_base(type* pointer = nullptr) {
		Claim(pointer);
	}
};

// Vin_ptr_own provides a similar system of ownership as with Rust.
template <typename type>
struct Vin_ptr_own : public Vin_ptr_base<type> {
public:
	void PassOwnershipTo(Vin_ptr_own<type>* passTo) {
		passTo->pointer = this->pointer;
		this->Disown();
	}
	using Vin_ptr_base<type>::Vin_ptr_base;
	~Vin_ptr_own() {
		this->Free();
	}
};

// Vin_ptr_borrow provides a similar system of borrowing as with Rust.
template <typename type>
struct Vin_ptr_borrow : Vin_ptr_base<Vin_ptr_own<type>> {
	type* GetPointer() {
		if (this->pointer != nullptr) return (type*)this->pointer;
		return nullptr;
	}
};

// Vin_ptr_counted is used for other pointers as a way of counting the references to it.
template <typename type>
struct Vin_ptr_counted : Vin_ptr_base<void*> {
public:
	std::mutex countLock;
	unsigned int count = 0;
	Vin_ptr_counted(type* pointer = NULL, unsigned int count = 0) {
		this->pointer = (void*)pointer;
		this->count = count;
	}
};

// Vin_ptr_counting_ptr_base is used as the base for Vinegar pointers that count the references to an area of memory.
template <typename type>
struct Vin_ptr_counting_ptr_base : public Vin_ptr_base<Vin_ptr_counted<type>> {
	void Add() {
		Vin_ptr_counted<type>* ptr = (Vin_ptr_counted<type>*)this->pointer;
		ptr->countLock.lock();
		ptr->count++;
		ptr->countLock.unlock();
	}
	void Subtract() {
		Vin_ptr_counted<type>* ptr = (Vin_ptr_counted<type>*)this->pointer;
		ptr->countLock.lock();
		ptr->count--;
		ptr->countLock.unlock();
	}
	void Store(Vin_ptr_counted<type>* pointer) {
		Vin_ptr_base<Vin_ptr_counted<type>>::Claim(pointer);
		if (pointer != nullptr) Add();
	}
	void Claim(Vin_ptr_counted<type>* pointer) {
		Subtract();
		Store();
	}
	void Free() {
		delete this->pointer->pointer;
		Vin_ptr_base<Vin_ptr_counted<type>>::Free();
	}
	bool IsDangling() {
		Vin_ptr_counted<type>* ptr = (Vin_ptr_counted<type>*)this->pointer;
		return ptr->count < 1;
	}
	void Disown() {
		Subtract();
	}
	Vin_ptr_counted<type>* GetPointer() {
		if (IsDangling()) return nullptr;
		return (Vin_ptr_counted<type>*)this->pointer;
	}
	Vin_ptr_counting_ptr_base(Vin_ptr_counted<type>* pointer = nullptr) {
		Store(pointer);
	}
};

// Vin_ptr_shared is a shared pointer where the memory will be freed when there are no more references to it.
template <typename type>
struct Vin_ptr_shared : public Vin_ptr_counting_ptr_base<type> { //Use struct containing int and pointer and use hashmap for garbage collection
public:
	void Disown() {
		Vin_ptr_counting_ptr_base<type>::Disown();
		if (Vin_ptr_counting_ptr_base<type>::IsDangling()) Vin_ptr_counting_ptr_base<type>::Free();
	}
	using Vin_ptr_counting_ptr_base<type>::Vin_ptr_counting_ptr_base;
	~Vin_ptr_shared() {
		Disown();
	}
};

std::mutex lock_GC;
std::vector<Vin_ptr_counted<void>*> toCollect;

template <typename type>
struct Vin_ptr_GC : public Vin_ptr_counting_ptr_base<type> {
public:
	void Disown() {
		Vin_ptr_counting_ptr_base<type>::Disown();
		if (Vin_ptr_counting_ptr_base<type>::IsDangling()) {
			lock_GC.lock();
			toCollect.push_back((Vin_ptr_counted<void>*)this->pointer);
			lock_GC.unlock();
		}
		Vin_ptr_counting_ptr_base<type>::Vin_ptr_base::Disown();
	}
	using Vin_ptr_counting_ptr_base<type>::Vin_ptr_counting_ptr_base;
	~Vin_ptr_GC() {
		Disown();
	}
};

void Vin_GC() {
	while (true) if (toCollect.size() != 0) {
		lock_GC.lock();
		delete toCollect[0]->pointer;
		delete toCollect[0];
		toCollect.erase(toCollect.begin());
		lock_GC.unlock();
	}
}

#endif
