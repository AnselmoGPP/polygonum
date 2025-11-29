#include "polygonum/bindings.hpp"

#include <iostream>

BindingSet::BindingSet() : r(nullptr) { }

BindingSet::BindingSet(const BindingSet& obj)
	: r(obj.r), vsGlobal(obj.vsGlobal), vsLocal(obj.vsLocal), vsTextures(obj.vsTextures), fsGlobal(obj.fsGlobal), fsLocal(obj.fsLocal), fsTextures(obj.fsTextures) {}

BindingSet& BindingSet::operator=(const BindingSet& obj)
{
	if (&obj == this) return *this;

	vsGlobal = obj.vsGlobal;
	vsLocal = obj.vsLocal;
	vsTextures = obj.vsTextures;
	fsGlobal = obj.fsGlobal;
	fsLocal = obj.fsLocal;
	fsTextures = obj.fsTextures;

	return *this;
}

BindingSet& BindingSet::operator=(BindingSet&& obj) noexcept
{
	if (this == &obj) return *this;

	r = std::move(obj.r);
	vsGlobal = std::move(obj.vsGlobal);
	vsLocal = std::move(obj.vsLocal);
	vsTextures = std::move(obj.vsTextures);
	fsGlobal = std::move(obj.fsGlobal);
	fsLocal = std::move(obj.fsLocal);
	fsTextures = std::move(obj.fsTextures);

	return *this;
}

void BindingSet::createBindings(Renderer* rend)
{
	r = rend;
	createTextures();
	createBuffers();
}

void BindingSet::clear()
{
	vsGlobal.clear();
	vsLocal.clear();
	vsTextures.clear();
	fsGlobal.clear();
	fsLocal.clear();
	fsTextures.clear();
}

void BindingSet::createTextures()
{
	for (auto& binding : vsTextures)
		for (auto& tex : binding)
			tex = tex->loadTexture(*r);

	for (auto& binding : fsTextures)
		for (auto& tex : binding)
			tex = tex->loadTexture(*r);
}

void BindingSet::createBuffers()
{
	for (BindingBuffer& ubo : vsLocal)
		ubo.createBuffer(r);

	for (BindingBuffer& ubo : fsLocal)
		ubo.createBuffer(r);
}

void BindingSet::destroyBuffers()
{
	for (BindingBuffer& ubo : vsLocal)
		ubo.destroyBuffer();

	for (BindingBuffer& ubo : fsLocal)
		ubo.destroyBuffer();
}

void BindingSet::clearBuffers()
{
	vsGlobal.clear();
	fsGlobal.clear();
	vsLocal.clear();
	fsLocal.clear();
}