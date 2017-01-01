/*
 * Copyright 2010-2011 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef WL_GRAPHIC_GL_UTILS_H
#define WL_GRAPHIC_GL_UTILS_H

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <stdint.h>

#include "base/log.h"
#include "base/macros.h"
#include "base/wexception.h"
#include "graphic/gl/system_headers.h"

namespace Gl {

class Shader;

// Returns the name of the 'error'.
const char* gl_error_to_string(GLenum error);

// Thin wrapper around a OpenGL program object to ensure proper cleanup. Throws
// on all errors.
class Program {
public:
	Program();
	~Program();

	GLuint object() const {
		return program_object_;
	}

	// Creates and compiles shader objects based on the corresponding files in data/shaders,
	// then links them into the program.
	void build(const std::string& program_name);
	void build_vp_fp(const std::vector<std::string>& vp_names,
	                 const std::vector<std::string>& fp_names);

private:
	const GLuint program_object_;

	DISALLOW_COPY_AND_ASSIGN(Program);
};

// Thin wrapper around a OpenGL buffer object to ensure proper cleanup. Throws
// on all errors. Also grows the server memory only when needed.
template <typename T> class Buffer {
public:
	Buffer() {
		glGenBuffers(1, &object_);
		if (!object_) {
			throw wexception("Could not create GL buffer.");
		}
	}

	~Buffer() {
		if (object_) {
			glDeleteBuffers(1, &object_);
		}
	}

	// Returns the OpenGL object for direct use.
	GLuint object() const {
		return object_;
	}

	// Calls glBindBuffer on the underlying buffer data.
	void bind() const {
		glBindBuffer(GL_ARRAY_BUFFER, object_);
	}

	// Copies 'elements' into the buffer, overwriting what was there before.
	// Does not check if the buffer is already bound.
	void update(const std::vector<T>& items) {
		// Always re-allocate the buffer. This ends up being much more
		// efficient than trying to do a partial update, because partial
		// updates tend to force the driver to do command buffer flushes.
		glBufferData(GL_ARRAY_BUFFER, items.size() * sizeof(T), items.data(), GL_DYNAMIC_DRAW);
	}

private:
	GLuint object_;

	DISALLOW_COPY_AND_ASSIGN(Buffer);
};

// Selected GL capabilities that are queried during initialization.
struct Capabilities {
	unsigned glsl_version = 0; // major * 100 + minor

	bool ARB_separate_shader_objects = false;
	bool ARB_shader_storage_buffer_object = false;
	bool ARB_uniform_buffer_object = false;

	void check();
};

// Some GL drivers do not remember the current pipeline state. If you rebind a
// texture that has already bound to the same target, they will happily stall
// the pipeline. We therefore cache the state of the GL driver in this class
// and skip unneeded GL calls.
class State {
public:
	static State& instance();

	void bind_framebuffer(GLuint framebuffer, GLuint texture);

	// Wrapper around glActiveTexture() and glBindTexture(). We never unbind a
	// texture, i.e. calls with texture == 0 are ignored.
	void bind(GLenum target, GLuint texture);

	void delete_texture(GLuint texture);

	// Calls glEnableVertexAttribArray on all 'entries' and disables all others
	// that are activated. 'entries' is taken by value on purpose.
	void enable_vertex_attrib_array(std::unordered_set<GLint> entries);

	const Capabilities& capabilities() const {
		return caps_;
	}

	void check_capabilities();

private:
	static const unsigned kMaxTextureTargets = 16;

	std::vector<GLuint> target_to_texture_;
	std::unordered_set<GLint> enabled_attrib_arrays_;
	GLenum last_active_texture_;
	GLuint current_framebuffer_;
	GLuint current_framebuffer_texture_;
	Capabilities caps_;

	State();

	void do_bind(GLenum target, GLuint texture);

	DISALLOW_COPY_AND_ASSIGN(State);
};

// Calls glVertexAttribPointer.
void vertex_attrib_pointer(int vertex_index, int num_items, int stride, int offset);

// Swap order of rows in pixels, to compensate for the upside-down nature of the
// OpenGL coordinate system.
void swap_rows(int width, int height, int pitch, int bpp, uint8_t* pixels);

}  // namespace Gl

#endif  // end of include guard: WL_GRAPHIC_GL_UTILS_H
