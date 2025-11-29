#ifndef BINDINGS_HPP
#define BINDINGS_HPP

#include "polygonum/ubo.hpp"
#include "polygonum/texture.hpp"

/*
	Bindings connect shader variables to data in GPU memory.
	A set of bindings is called "descriptor set".
	A binding contains a set of descriptors of the same type.
	Each descriptor is a buffer (ubo or ssbo) or texture.
	Each binding is passed to a single shader (vertex shader, fragment shader).
*/

class Texture;

/// Set of bindings. Each binding has a set of descriptors (buffers or textures).
struct BindingSet
{
	BindingSet();
	BindingSet(const BindingSet& obj);
	BindingSet& operator=(const BindingSet& obj);
	BindingSet& operator=(BindingSet&& obj) noexcept;   //!< Move assignment operator

	Renderer* r;

	vec<BindingBuffer*> vsGlobal;   //!< [binding]
	vec<BindingBuffer> vsLocal;   //!< [binding]
	vec2<std::shared_ptr<Texture>> vsTextures;   //!< [binding][descriptor]

	vec<BindingBuffer*> fsGlobal;
	vec<BindingBuffer> fsLocal;
	vec2<std::shared_ptr<Texture>> fsTextures;

	void createBindings(Renderer* rend);
	void clear();

private:
	void createTextures();
	void createBuffers();
	void destroyBuffers();
	void clearBuffers();
};

#endif