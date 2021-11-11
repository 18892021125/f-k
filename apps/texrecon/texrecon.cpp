/*
 * Copyright (C) 2015, Nils Moehrle
 * TU Darmstadt - Graphics, Capture and Massively Parallel Computing
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD 3-Clause license. See the LICENSE.txt file for details.
 */

#include <iostream>
#include <fstream>
#include <vector>

#include <util/timer.h>
#include <util/system.h>
#include <util/file_system.h>
#include <mve/mesh_io_ply.h>
#include <glm/glm.hpp>

#include "tex/util.h"
#include "tex/timer.h"
#include "tex/debug.h"
#include "tex/texturing.h"
#include "tex/progress_counter.h"

#include "arguments.h"

int main(int argc, char **argv) {
    util::system::print_build_timestamp(argv[0]);
    util::system::register_segfault_handler();

#ifdef RESEARCH
    std::cout << "******************************************************************************" << std::endl
              << " Due to use of the -DRESEARCH=ON compile option, this program is licensed "     << std::endl
              << " for research purposes only. Please pay special attention to the gco license."  << std::endl
              << "******************************************************************************" << std::endl;
#endif

    Timer timer;
    util::WallTimer wtimer;

    Arguments conf;
    try {
        conf = parse_args(argc, argv);
    } catch (std::invalid_argument & ia) {
        std::cerr << ia.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }

    if (!util::fs::dir_exists(util::fs::dirname(conf.out_prefix).c_str())) {
        std::cerr << "Destination directory does not exist!" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    std::cout << "Load and prepare mesh: " << std::endl;
    mve::TriangleMesh::Ptr mesh;
    try {
        mesh = mve::geom::load_ply_mesh(conf.in_mesh);
    } catch (std::exception& e) {
        std::cerr << "\tCould not load mesh: "<< e.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
    mve::MeshInfo mesh_info(mesh);
    tex::prepare_mesh(&mesh_info, mesh);

    std::cout << "Generating texture views: " << std::endl;
    tex::TextureViews texture_views;
    tex::generate_texture_views(conf.in_scene, &texture_views);

    write_string_to_file(conf.out_prefix + ".conf", conf.to_string());
    timer.measure("Loading");

    std::size_t const num_faces = mesh->get_faces().size() / 3;

    std::cout << "Building adjacency graph: " << std::endl;
    tex::Graph graph(num_faces);
    tex::build_adjacency_graph(mesh, mesh_info, &graph);

    if (conf.labeling_file.empty()) {
        std::cout << "View selection:" << std::endl;
        util::WallTimer rwtimer;

        tex::DataCosts data_costs(num_faces, texture_views.size());
        if (conf.data_cost_file.empty()) {
            tex::calculate_data_costs(mesh, &texture_views, conf.settings, &data_costs);

            if (conf.write_intermediate_results) {
                std::cout << "\tWriting data cost file... " << std::flush;
                tex::DataCosts::save_to_file(data_costs, conf.out_prefix + "_data_costs.spt");
                std::cout << "done." << std::endl;
            }
        } else {
            std::cout << "\tLoading data cost file... " << std::flush;
            try {
                tex::DataCosts::load_from_file(conf.data_cost_file, &data_costs);
            } catch (util::FileException e) {
                std::cout << "failed!" << std::endl;
                std::cerr << e.what() << std::endl;
                std::exit(EXIT_FAILURE);
            }
            std::cout << "done." << std::endl;
        }
        timer.measure("Calculating data costs");

        tex::view_selection(data_costs, &graph, conf.settings);
        timer.measure("Running MRF optimization");
        std::cout << "\tTook: " << rwtimer.get_elapsed_sec() << "s" << std::endl;

        /* Write labeling to file. */
        if (conf.write_intermediate_results) {
            std::vector<std::size_t> labeling(graph.num_nodes());
            for (std::size_t i = 0; i < graph.num_nodes(); ++i) {
                labeling[i] = graph.get_label(i);
            }
            vector_to_file(conf.out_prefix + "_labeling.vec", labeling);
        }
    } else {
        std::cout << "Loading labeling from file... " << std::flush;

        /* Load labeling from file. */
        std::vector<std::size_t> labeling = vector_from_file<std::size_t>(conf.labeling_file);
        if (labeling.size() != graph.num_nodes()) {
            std::cerr << "Wrong labeling file for this mesh/scene combination... aborting!" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        /* Transfer labeling to graph. */
        for (std::size_t i = 0; i < labeling.size(); ++i) {
            const std::size_t label = labeling[i];
            if (label > texture_views.size()){
                std::cerr << "Wrong labeling file for this mesh/scene combination... aborting!" << std::endl;
                std::exit(EXIT_FAILURE);
            }
            graph.set_label(i, label);
        }

        std::cout << "done." << std::endl;
    }

    tex::TextureAtlases texture_atlases;
    {
        /* Create texture patches and adjust them. */
        tex::TexturePatches texture_patches;
        tex::VertexProjectionInfos vertex_projection_infos;
        std::cout << "Generating texture patches:" << std::endl;
        tex::generate_texture_patches(graph, mesh, mesh_info, &texture_views,
            conf.settings, &vertex_projection_infos, &texture_patches);

        if (conf.settings.global_seam_leveling) {
            std::cout << "Running global seam leveling:" << std::endl;
            tex::global_seam_leveling(graph, mesh, mesh_info, vertex_projection_infos, &texture_patches);
            timer.measure("Running global seam leveling");
        } else {
            ProgressCounter texture_patch_counter("Calculating validity masks for texture patches", texture_patches.size());
            #pragma omp parallel for schedule(dynamic)
#if !defined(_MSC_VER)
            for (std::size_t i = 0; i < texture_patches.size(); ++i) {
#else
            for (std::int64_t i = 0; i < texture_patches.size(); ++i) {
#endif
                texture_patch_counter.progress<SIMPLE>();
                TexturePatch::Ptr texture_patch = texture_patches[i];
                std::vector<math::Vec3f> patch_adjust_values(texture_patch->get_faces().size() * 3, math::Vec3f(0.0f));
                texture_patch->adjust_colors(patch_adjust_values);
                texture_patch_counter.inc();
            }
            timer.measure("Calculating texture patch validity masks");
        }

        if (conf.settings.local_seam_leveling) {
            std::cout << "Running local seam leveling:" << std::endl;
            tex::local_seam_leveling(graph, mesh, vertex_projection_infos, &texture_patches);
        }
        timer.measure("Running local seam leveling");

        /* Generate texture atlases. */
        std::cout << "Generating texture atlases:" << std::endl;
        tex::generate_texture_atlases(&texture_patches, conf.settings, &texture_atlases);
    }

    /* Create and write out obj model. */
    {
        std::cout << "Building objmodel:" << std::endl;
        tex::Model model;
        tex::build_model(mesh, texture_atlases, &model);
        timer.measure("Building OBJ model");

        std::cout << "\tSaving model... " << std::flush;
        tex::Model::save(model, conf.out_prefix);
        std::cout << "done." << std::endl;
        timer.measure("Saving");
    }

    std::cout << "Whole texturing procedure took: " << wtimer.get_elapsed_sec() << "s" << std::endl;
    timer.measure("Total");
    if (conf.write_timings) {
        timer.write_to_file(conf.out_prefix + "_timings.csv");
    }

    if (conf.write_view_selection_model) {
        texture_atlases.clear();
        std::cout << "Generating debug texture patches:" << std::endl;
        {
            tex::TexturePatches texture_patches;
            generate_debug_embeddings(&texture_views);
            tex::VertexProjectionInfos vertex_projection_infos; // Will only be written
            tex::generate_texture_patches(graph, mesh, mesh_info, &texture_views,
                conf.settings, &vertex_projection_infos, &texture_patches);
            tex::generate_texture_atlases(&texture_patches, conf.settings, &texture_atlases);
        }

        std::cout << "Building debug objmodel:" << std::endl;
        {
            tex::Model model;
            tex::build_model(mesh, texture_atlases, &model);
            std::cout << "\tSaving model... " << std::flush;
            tex::Model::save(model, conf.out_prefix + "_view_selection");
            std::cout << "done." << std::endl;
        }
    }

    return EXIT_SUCCESS;
}

void build_model(mve::TriangleMesh::ConstPtr mesh,
	std::vector<TextureAtlas::Ptr> const & texture_atlases, 
	std::vector<glm::vec3> &points,
	std::vector<glm::vec3> &normals,
	std::vector<glm::vec2> &texCoords,
	std::vector<glm::ivec3> &triangles,
	int &textureWidth, int &textureHeight, 
	std::vector<uint8_t> &textureData)
{
	const auto &mesh_vertices = mesh->get_vertices();
	const auto &mesh_normals = mesh->get_vertex_normals();
	const auto &mesh_faces = mesh->get_faces();

	if (!texture_atlases.empty())
	{
		const auto &texture_atlas = texture_atlases.front();
		const auto &image = texture_atlas->get_image();
		textureWidth = image->width();
		textureHeight = image->height();
		textureData.resize(textureWidth * textureHeight * 3);
		memcpy(textureData.data(), image->get_data_pointer(), textureData.size());

		const auto &atlas_faces = texture_atlas->get_faces();
		const auto &atlas_texcoords = texture_atlas->get_texcoords();
		const auto &atlas_texcoord_ids = texture_atlas->get_texcoord_ids();

		texCoords.resize(atlas_texcoords.size());
		memcpy(texCoords.data(), atlas_texcoords.data(), texCoords.size() * sizeof(glm::vec2));

		triangles.resize(atlas_faces.size());
		std::vector<glm::ivec3> vertexIndices(atlas_faces.size());
		for (auto i = 0; i < atlas_faces.size(); ++i)
		{
			memcpy(vertexIndices.data() + i, mesh_faces.data() + atlas_faces[i] * 3, sizeof(glm::ivec3));
			//atlas_texcoord_ids.dataÊÇinit64_t£¬²»ÄÜmemcpy
			for (auto j = 0; j < 3; ++j)
				triangles[i][j] = atlas_texcoord_ids.data()[i * 3 + j];
		}
		std::vector<int> texCoordVertexIndices(texCoords.size());
		for (auto i = 0; i < triangles.size(); ++i)
			for (auto j = 0; j < 3; ++j)
				texCoordVertexIndices[triangles[i][j]] = vertexIndices[i][j];
		points.resize(texCoords.size());
		for (auto i = 0; i < points.size(); ++i)
			memcpy(points.data() + i, mesh_vertices.data() + texCoordVertexIndices[i], sizeof(glm::vec3));
		normals.resize(texCoords.size());
		for (auto i = 0; i < normals.size(); ++i)
			memcpy(normals.data() + i, mesh_normals.data() + texCoordVertexIndices[i], sizeof(glm::vec3));
	}
}

__declspec(dllexport) std::string reconstructTexture(
	const int &width, const int &height, 
	const std::vector<std::vector<uint8_t>> &imagesData,
	const std::vector<std::vector<float>> &camerasIntrinsic,
	const std::vector<std::vector<float>> &camerasExtrinsic, 
	std::vector<glm::vec3> &points, 
	std::vector<glm::vec3> &normals, 
	std::vector<glm::vec2> &texCoords, 
	std::vector<glm::ivec3> &triangles, 
	int &textureWidth, int &textureHeight,
	std::vector<uint8_t> &textureData)
{
	util::system::register_segfault_handler();

#ifdef RESEARCH
	std::cout << "******************************************************************************" << std::endl
		<< " Due to use of the -DRESEARCH=ON compile option, this program is licensed " << std::endl
		<< " for research purposes only. Please pay special attention to the gco license." << std::endl
		<< "******************************************************************************" << std::endl;
#endif

	Timer timer;
	util::WallTimer wtimer;

	Arguments conf;
	conf.out_prefix = "textured";
	conf.write_timings = false;
	conf.write_intermediate_results = false;
	conf.write_view_selection_model = false;

	std::cout << "Prepare mesh: " << std::endl;
	auto mesh = mve::TriangleMesh::create();
	mesh->get_vertices().resize(points.size());
	memcpy(mesh->get_vertices().data(), points.data(), points.size() * sizeof(glm::vec3));
	mesh->get_vertex_normals().resize(normals.size());
	memcpy(mesh->get_vertex_normals().data(), normals.data(), normals.size() * sizeof(glm::vec3));
	mesh->get_faces().resize(triangles.size() * 3);
	memcpy(mesh->get_faces().data(), triangles.data(), triangles.size() * sizeof(glm::ivec3));
	mve::MeshInfo mesh_info(mesh);
	tex::prepare_mesh(&mesh_info, mesh);

	std::cout << "Generating texture views: " << std::endl;
	tex::TextureViews texture_views;
	tex::generate_texture_views(width, height, imagesData, camerasIntrinsic, camerasExtrinsic, &texture_views);

	//write_string_to_file(conf.out_prefix + ".conf", conf.to_string());
	timer.measure("Loading");

	std::size_t const num_faces = mesh->get_faces().size() / 3;

	std::cout << "Building adjacency graph: " << std::endl;
	tex::Graph graph(num_faces);
	tex::build_adjacency_graph(mesh, mesh_info, &graph);

	if (conf.labeling_file.empty()) {
		std::cout << "View selection:" << std::endl;
		util::WallTimer rwtimer;

		tex::DataCosts data_costs(num_faces, texture_views.size());
		if (conf.data_cost_file.empty()) {
			tex::calculate_data_costs(mesh, &texture_views, conf.settings, &data_costs);

			if (conf.write_intermediate_results) {
				std::cout << "\tWriting data cost file... " << std::flush;
				tex::DataCosts::save_to_file(data_costs, conf.out_prefix + "_data_costs.spt");
				std::cout << "done." << std::endl;
			}
		}
		else {
			std::cout << "\tLoading data cost file... " << std::flush;
			try {
				tex::DataCosts::load_from_file(conf.data_cost_file, &data_costs);
			}
			catch (util::FileException e) {
				return std::string("failed!\n") + e.what() + '\n';
			}
			std::cout << "done." << std::endl;
		}
		timer.measure("Calculating data costs");

		tex::view_selection(data_costs, &graph, conf.settings);
		timer.measure("Running MRF optimization");
		std::cout << "\tTook: " << rwtimer.get_elapsed_sec() << "s" << std::endl;

		/* Write labeling to file. */
		if (conf.write_intermediate_results) {
			std::vector<std::size_t> labeling(graph.num_nodes());
			for (std::size_t i = 0; i < graph.num_nodes(); ++i) {
				labeling[i] = graph.get_label(i);
			}
			vector_to_file(conf.out_prefix + "_labeling.vec", labeling);
		}
	}
	else {
		std::cout << "Loading labeling from file... " << std::flush;

		/* Load labeling from file. */
		std::vector<std::size_t> labeling = vector_from_file<std::size_t>(conf.labeling_file);
		if (labeling.size() != graph.num_nodes())
			return std::string("Wrong labeling file for this mesh/scene combination... aborting!\n") + '\n';

		/* Transfer labeling to graph. */
		for (std::size_t i = 0; i < labeling.size(); ++i) {
			const std::size_t label = labeling[i];
			if (label > texture_views.size())
				return std::string("Wrong labeling file for this mesh/scene combination... aborting!\n") + '\n';
			graph.set_label(i, label);
		}

		std::cout << "done." << std::endl;
	}

	tex::TextureAtlases texture_atlases;
	{
		/* Create texture patches and adjust them. */
		tex::TexturePatches texture_patches;
		tex::VertexProjectionInfos vertex_projection_infos;
		std::cout << "Generating texture patches:" << std::endl;
		tex::generate_texture_patches(graph, mesh, mesh_info, &texture_views,
			conf.settings, &vertex_projection_infos, &texture_patches);

		if (conf.settings.global_seam_leveling) {
			std::cout << "Running global seam leveling:" << std::endl;
			tex::global_seam_leveling(graph, mesh, mesh_info, vertex_projection_infos, &texture_patches);
			timer.measure("Running global seam leveling");
		}
		else {
			ProgressCounter texture_patch_counter("Calculating validity masks for texture patches", texture_patches.size());
#pragma omp parallel for schedule(dynamic)
#if !defined(_MSC_VER)
			for (std::size_t i = 0; i < texture_patches.size(); ++i) {
#else
			for (std::int64_t i = 0; i < texture_patches.size(); ++i) {
#endif
				texture_patch_counter.progress<SIMPLE>();
				TexturePatch::Ptr texture_patch = texture_patches[i];
				std::vector<math::Vec3f> patch_adjust_values(texture_patch->get_faces().size() * 3, math::Vec3f(0.0f));
				texture_patch->adjust_colors(patch_adjust_values);
				texture_patch_counter.inc();
			}
			timer.measure("Calculating texture patch validity masks");
			}

		if (conf.settings.local_seam_leveling) {
			std::cout << "Running local seam leveling:" << std::endl;
			tex::local_seam_leveling(graph, mesh, vertex_projection_infos, &texture_patches);
		}
		timer.measure("Running local seam leveling");

		/* Generate texture atlases. */
		std::cout << "Generating texture atlases:" << std::endl;
		tex::generate_texture_atlases(&texture_patches, conf.settings, &texture_atlases);
		}

	/* Create and write out obj model. */
	{
		std::cout << "Building model:" << std::endl;
		tex::Model model;
		::build_model(mesh, texture_atlases, points, normals, texCoords, triangles, textureWidth, textureHeight, textureData);
		timer.measure("Building model");
	}

	std::cout << "Whole texturing procedure took: " << wtimer.get_elapsed_sec() << "s" << std::endl;
	timer.measure("Total");
	if (conf.write_timings) {
		timer.write_to_file(conf.out_prefix + "_timings.csv");
	}

	if (conf.write_view_selection_model) {
		texture_atlases.clear();
		std::cout << "Generating debug texture patches:" << std::endl;
		{
			tex::TexturePatches texture_patches;
			generate_debug_embeddings(&texture_views);
			tex::VertexProjectionInfos vertex_projection_infos; // Will only be written
			tex::generate_texture_patches(graph, mesh, mesh_info, &texture_views,
				conf.settings, &vertex_projection_infos, &texture_patches);
			tex::generate_texture_atlases(&texture_patches, conf.settings, &texture_atlases);
		}

		std::cout << "Building debug objmodel:" << std::endl;
		{
			tex::Model model;
			tex::build_model(mesh, texture_atlases, &model);
			std::cout << "\tSaving model... " << std::flush;
			tex::Model::save(model, conf.out_prefix + "_view_selection");
			std::cout << "done." << std::endl;
		}
	}

	return "";
}