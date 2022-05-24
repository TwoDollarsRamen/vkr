#include <string.h> /* memcpy */

#include "renderer.hpp"
#include "vkr.hpp"

static const vkr::u32 default_texture_data[2][2] = {
	0xff00ffff, 0x000000ff,
	0x000000ff, 0xff00ffff
};

namespace vkr {
	PostProcessStep::PostProcessStep(
			Renderer3D* renderer, Shader* shader,
			Dependency* dependencies, usize dependency_count,
			bool use_default_fb) : framebuffer(null), use_default_fb(use_default_fb),
			dependency_count(dependency_count), renderer(renderer) {

		if (!use_default_fb) {
			Framebuffer::Attachment attachments[] = {
				{
					.type = Framebuffer::Attachment::Type::color,
					.format = Framebuffer::Attachment::Format::rgbaf16,
				}
			};

			framebuffer = new Framebuffer(renderer->app->video,
				Framebuffer::Flags::headless | Framebuffer::Flags::fit,
				renderer->app->get_size(), attachments, 1);
		} else {
			framebuffer = renderer->app->get_default_framebuffer();
		}


		Pipeline::Attribute post_attribs[] = {
			{
				.name = "position",
				.location = 0,
				.offset = 0,
				.type = Pipeline::Attribute::Type::float2
			},
			{
				.name = "uv",
				.location = 1,
				.offset = sizeof(v2f),
				.type = Pipeline::Attribute::Type::float2
			},
		};


		Pipeline::Descriptor uniform_desc{};
		uniform_desc.name = "fragment_uniform_buffer";
		uniform_desc.binding = 0;
		uniform_desc.stage = Pipeline::Stage::fragment;
		uniform_desc.resource.type = Pipeline::ResourcePointer::Type::uniform_buffer;
		uniform_desc.resource.uniform.ptr = &renderer->f_post_ub;
		uniform_desc.resource.uniform.size = sizeof(renderer->f_post_ub);

		auto sampler_descs = new Pipeline::Descriptor[dependency_count]();
		for (usize i = 0; i < dependency_count; i++) {
			sampler_descs[i].name = "input";
			sampler_descs[i].binding = i;
			sampler_descs[i].stage = Pipeline::Stage::fragment;
			sampler_descs[i].resource.type = Pipeline::ResourcePointer::Type::framebuffer_output;
			sampler_descs[i].resource.framebuffer.ptr = dependencies[i].framebuffer;
			sampler_descs[i].resource.framebuffer.attachment = dependencies[i].attachment;
		}

		Pipeline::DescriptorSet desc_sets[] = {
			{
				.name = "uniforms",
				.descriptors = &uniform_desc,
				.count = 1,
			},
			{
				.name = "samplers",
				.descriptors = sampler_descs,
				.count = dependency_count
			}
		};

		pipeline = new Pipeline(renderer->app->video,
			Pipeline::Flags::cull_back_face,
			shader,
			sizeof(v2f) * 2,
			post_attribs, 2,
			framebuffer,
			desc_sets, 2);
	}

	PostProcessStep::~PostProcessStep() {
		delete pipeline;
		if (use_default_fb) {
			delete framebuffer;
		}
	}

	void PostProcessStep::execute() {
		pipeline->begin();
		pipeline->bind_descriptor_set(0, 0);
		pipeline->bind_descriptor_set(1, 1);

		renderer->fullscreen_tri->bind();
		renderer->fullscreen_tri->draw(3);
		pipeline->end();
	}

	Renderer3D::Renderer3D(App* app, VideoContext* video, const ShaderConfig& shaders, Material* materials, usize material_count) :
		app(app), model(null) {

		default_texture = new Texture(video, (const void*)default_texture_data, v2i(2, 2), 4);

		Framebuffer::Attachment attachments[] = {
			{
				.type = Framebuffer::Attachment::Type::color,
				.format = Framebuffer::Attachment::Format::rgbaf16,
			},
			{
				.type = Framebuffer::Attachment::Type::depth,
				.format = Framebuffer::Attachment::Format::depth,
			},
		};

		scene_fb = new Framebuffer(video,
			Framebuffer::Flags::headless | Framebuffer::Flags::fit,
			app->get_size(), attachments, 2);

		Pipeline::Attribute attribs[] = {
			{
				.name     = "position",
				.location = 0,
				.offset   = offsetof(Vertex, position),
				.type     = Pipeline::Attribute::Type::float3
			},
			{
				.name     = "uv",
				.location = 1,
				.offset   = offsetof(Vertex, uv),
				.type     = Pipeline::Attribute::Type::float2
			},
			{
				.name     = "normal",
				.location = 2,
				.offset   = offsetof(Vertex, normal),
				.type     = Pipeline::Attribute::Type::float3
			},
			{
				.name     = "tangent",
				.location = 3,
				.offset   = offsetof(Vertex, tangent),
				.type     = Pipeline::Attribute::Type::float3
			},
			{
				.name     = "bitangent",
				.location = 4,
				.offset   = offsetof(Vertex, bitangent),
				.type     = Pipeline::Attribute::Type::float3
			},
		};

		Pipeline::PushConstantRange pc[] = {
			{
				.name = "transform",
				.size = sizeof(v_pc),
				.start = 0,
				.stage = Pipeline::Stage::vertex
			},
			{
				.name = "frag_data",
				.size = sizeof(f_pc),
				.start = sizeof(v_pc),
				.stage = Pipeline::Stage::fragment
			}
		};

		this->materials = new Material[material_count]();
		memcpy(this->materials, materials, material_count * sizeof(Material));

		Pipeline::Descriptor uniform_descs[2];
		uniform_descs[0].name = "vertex_uniform_buffer";
		uniform_descs[0].binding = 0;
		uniform_descs[0].stage = Pipeline::Stage::vertex;
		uniform_descs[0].resource.type = Pipeline::ResourcePointer::Type::uniform_buffer;
		uniform_descs[0].resource.uniform.ptr  = &v_ub;
		uniform_descs[0].resource.uniform.size = sizeof(v_ub);

		uniform_descs[1].name = "vertex_uniform_buffer";
		uniform_descs[1].binding = 1;
		uniform_descs[1].stage = Pipeline::Stage::fragment;
		uniform_descs[1].resource.type = Pipeline::ResourcePointer::Type::uniform_buffer;
		uniform_descs[1].resource.uniform.ptr  = &f_ub;
		uniform_descs[1].resource.uniform.size = sizeof(f_ub);

		auto desc_sets = new Pipeline::DescriptorSet[1 + material_count]();
		desc_sets[0].name = "uniforms";
		desc_sets[0].descriptors = uniform_descs;
		desc_sets[0].count = 2;

		for (usize i = 0; i < material_count; i++) {
			auto set = desc_sets + i + 1;

			auto tex_descs = new Pipeline::Descriptor[Material::get_texture_count()]();

			tex_descs[0].name = "diffuse";
			tex_descs[0].binding = 0;
			tex_descs[0].stage = Pipeline::Stage::fragment;
			tex_descs[0].resource.type = Pipeline::ResourcePointer::Type::texture;
			tex_descs[0].resource.texture.ptr = materials[i].diffuse != null ? materials[i].diffuse : default_texture;

			tex_descs[1].name = "normal";
			tex_descs[1].binding = 1;
			tex_descs[1].stage = Pipeline::Stage::fragment;
			tex_descs[1].resource.type = Pipeline::ResourcePointer::Type::texture;
			tex_descs[1].resource.texture.ptr = materials[i].normal != null ? materials[i].normal : default_texture;

			set->descriptors = tex_descs;
			set->count = Material::get_texture_count();
		}

		scene_pip = new Pipeline(video,
			Pipeline::Flags::depth_test |
			Pipeline::Flags::cull_back_face,
			shaders.lit,
			sizeof(Vertex),
			attribs, 5,
			scene_fb,
			desc_sets, material_count + 1,
			pc, 2);

		v2f tri_verts[] = {
			/* Position          UV */
			{ -1.0, -1.0 },      { 0.0f, 0.0f },
			{ -1.0,  3.0 },      { 0.0f, 2.0f },
			{  3.0, -1.0 },      { 2.0f, 0.0f }
		};

		fullscreen_tri = new VertexBuffer(video, tri_verts, sizeof(tri_verts));

		PostProcessStep::Dependency tonemap_deps[] = {
			{
				.name = "color",
				.framebuffer = scene_fb,
				.attachment = 0
			}
		};

		tonemap = new PostProcessStep(this, shaders.tonemap, tonemap_deps, 1, true);

		for (usize i = 0; i < material_count; i++) {
			delete[] desc_sets[i + 1].descriptors;
		}

		delete[] desc_sets;
	}

	Renderer3D::~Renderer3D() {
		delete fullscreen_tri;
		delete scene_fb;
		delete tonemap;
		delete scene_pip;

		delete[] materials;

		delete default_texture;
	}

	void Renderer3D::draw(ecs::World* world) {
		auto size = app->get_size();

		v3f camera_pos(0.0f, 0.0f, -5.0f);

		v_ub.projection = m4f::pers(70.0f, (f32)size.x / (f32)size.y, 0.1f, 100.0f);
		v_ub.view = m4f::translate(m4f::identity(), camera_pos);

		f_ub.camera_pos = camera_pos;

		f_ub.point_light_count = 0;
		for (ecs::View view = world->new_view<Transform, PointLight>(); view.valid(); view.next()) {
			auto& trans = view.get<Transform>();
			auto& light = view.get<PointLight>();

			auto idx = f_ub.point_light_count++;
			f_ub.point_lights[idx].intensity = light.intensity;
			f_ub.point_lights[idx].diffuse = light.diffuse;
			f_ub.point_lights[idx].specular = light.specular;
			f_ub.point_lights[idx].position = trans.m.get_translation();
			f_ub.point_lights[idx].range = light.range;
		}

		f_ub.sun.direction = sun.direction;
		f_ub.sun.intensity = sun.intensity;
		f_ub.sun.diffuse = sun.diffuse;
		f_ub.sun.specular = sun.specular;

		scene_pip->begin();

		for (auto view = world->new_view<Transform, Renderable3D>(); view.valid(); view.next()) {
			auto& trans = view.get<Transform>();
			auto& renderable = view.get<Renderable3D>();
			
			auto model = renderable.model;
			auto material_id = renderable.material_id;

			this->model = model;

			scene_pip->bind_descriptor_set(0, 0);
			scene_pip->bind_descriptor_set(1, 1 + material_id);

			f_pc.use_diffuse_map = materials[material_id].diffuse == null ? 0.0f : 1.0f;
			f_pc.use_normal_map = materials[material_id].normal == null ? 0.0f : 1.0f;

			v_pc.transform = trans.m;
			for (auto mesh : model->meshes) {
				scene_pip->push_constant(Pipeline::Stage::vertex, v_pc);
				scene_pip->push_constant(Pipeline::Stage::fragment, f_pc, sizeof(v_pc));
				mesh->vb->bind();
				mesh->ib->draw();
			}
		}

		scene_pip->end();

		tonemap->execute();
	}

	Mesh3D* Mesh3D::from_wavefront(VideoContext* video, WavefrontModel* wmodel, WavefrontModel::Mesh* wmesh) {
		auto verts = new Renderer3D::Vertex[wmesh->vertices.size()];
		auto indices = new u16[wmesh->vertices.size()];

		usize vert_count = 0;
		usize index_count = 0;

		for (auto vertex : wmesh->vertices) {
			auto pos = wmodel->positions[vertex.position];
			auto normal = wmodel->normals[vertex.normal];
			auto uv = wmodel->uvs[vertex.uv];

			bool is_new = true;

			for (usize i = 0; i < vert_count; i++) {
				if (
					pos    == verts[i].position &&
					normal == verts[i].normal &&
					uv     == verts[i].uv) {
					indices[index_count++] = i;
					is_new = false;
				}
			}

			if (is_new) {
				verts[vert_count].position = pos;
				verts[vert_count].normal = normal;
				verts[vert_count].uv = uv;
				indices[index_count++] = vert_count;
				vert_count++;
			}
		}

		for (u32 i = 0; i < index_count; i += 3) {
			v3f pos1 = verts[indices[i + 0]].position;
			v3f pos2 = verts[indices[i + 1]].position;
			v3f pos3 = verts[indices[i + 2]].position;

			v2f uv1 = verts[indices[i + 0]].uv;
			v2f uv2 = verts[indices[i + 1]].uv;
			v2f uv3 = verts[indices[i + 2]].uv;

			v2f delta_uv_1 = uv2 - uv1;
			v2f delta_uv_2 = uv3 - uv1;

			v3f edge_1 = pos2 - pos1;
			v3f edge_2 = pos3 - pos1;

			f32 f = 1.0f / (delta_uv_1.x * delta_uv_2.y - delta_uv_2.x * delta_uv_1.y);

			v3f tangent = {
				f * (delta_uv_2.y * edge_1.x - delta_uv_1.y * edge_2.x),
				f * (delta_uv_2.y * edge_1.y - delta_uv_1.y * edge_2.y),
				f * (delta_uv_2.y * edge_1.z - delta_uv_1.y * edge_2.z)
			};

			v3f bitangent = {
				f * (-delta_uv_2.x * edge_1.x + delta_uv_1.x * edge_2.x),
				f * (-delta_uv_2.x * edge_1.y + delta_uv_1.x * edge_2.y),
				f * (-delta_uv_2.x * edge_1.z + delta_uv_1.x * edge_2.z)
			};

			verts[indices[i + 0]].tangent = tangent;
			verts[indices[i + 1]].tangent = tangent;
			verts[indices[i + 2]].tangent = tangent;

			verts[indices[i + 0]].bitangent = bitangent;
			verts[indices[i + 1]].bitangent = bitangent;
			verts[indices[i + 2]].bitangent = bitangent;
		}

		Mesh3D* r = new Mesh3D();

		r->vb = new VertexBuffer(video, verts, vert_count * sizeof(Renderer3D::Vertex));
		r->ib = new IndexBuffer(video, indices, index_count);

		delete[] verts;
		delete[] indices;

		return r;
	}

	Mesh3D::~Mesh3D() {
		delete vb;
		delete ib;
	}

	Model3D* Model3D::from_wavefront(VideoContext* video, WavefrontModel* wmodel) {
		Model3D* model = new Model3D;

		if (wmodel->has_root_mesh) {
			model->meshes.push_back(Mesh3D::from_wavefront(video, wmodel, &wmodel->root_mesh));
		}

		for (auto& mesh : wmodel->meshes) {
			model->meshes.push_back(Mesh3D::from_wavefront(video, wmodel, &mesh));
		}

		return model;
	}

	Model3D::~Model3D() {
		for (auto mesh : meshes) {
			delete mesh;
		}
	}
}
