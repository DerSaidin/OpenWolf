/*
===========================================================================

OpenWolf GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the OpenWolf GPL Source Code (OpenWolf Source Code).  

OpenWolf Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

OpenWolf Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenWolf Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the OpenWolf Source Code is also subject to certain additional terms. 
You should have received a copy of these additional terms immediately following the 
terms and conditions of the GNU General Public License which accompanied the OpenWolf 
Source Code.  If not, please request a copy in writing from id Software at the address 
below.

If you have questions concerning this license or the applicable additional terms, you 
may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, 
Maryland 20850 USA.

===========================================================================
*/

#include "../idLib/precompiled.h"
#pragma hdrstop

#include "CollisionModel_local.h"

extern cm_windingList_t *				cm_windingList;
extern cm_windingList_t *				cm_outList;
extern cm_windingList_t *				cm_tmpList;

extern idHashIndex *					cm_vertexHash;
extern idHashIndex *					cm_edgeHash;

extern idBounds							cm_modelBounds;
extern int								cm_vertexShift;

// load collision models from a Q3 BSP file
void idCollisionModelManagerLocal::LoadMap( const char *fname, bool clientload, int *checksum ) {

	if ( fname == NULL ) {
		Com_FatalError( "idCollisionModelManagerLocal::LoadMap: NULL fname" );
	}

	// check whether we can keep the current collision map based on the mapName and mapFileTime
	if ( loaded ) {
		if ( mapName.Icmp( fname ) == 0 ) {
			if ( 0 == mapFileTime ) {
				Com_DPrintf( "Using loaded version\n" );
				return;
			}
			Com_DPrintf( "Reloading modified map\n" );
		}
		FreeMap();
	}

	// clear the collision map
	Clear();

	// models
	maxModels = MAX_SUBMODELS;
	numModels = 0;
	models = (cm_model_t **) Mem_ClearedAlloc( (maxModels+1) * sizeof(cm_model_t *) );

	// setup hash to speed up finding shared vertices and edges
	SetupHash();

	// setup trace model structure
	SetupTrmModelStructure();

	// build collision models
	BuildModelsBSP( fname );

	// save name and time stamp
	mapName = fname;
	mapFileTime = 0;
	loaded = true;

	// shutdown the hash
	ShutdownHash();
}
void idCollisionModelManagerLocal::ParseBSPNodes( dnode_t *nodes, int numNodes, dplane_t *planes ) {
	int i;

	numProcNodes = numNodes;

	procNodes = (cm_procNode_t *)Mem_ClearedAlloc( numProcNodes * sizeof( cm_procNode_t ) );
	dnode_t *in = nodes;
	for ( i = 0; i < numProcNodes; i++, in++ ) {
		cm_procNode_t *node;
		dplane_t *pl;

		node = &procNodes[i];
		pl = &planes[in->planeNum];

		node->plane.SetDist(pl->dist);
		node->plane.SetNormal(pl->normal);

		node->children[0] = in->children[0];
		node->children[1] = in->children[1];
	}
}
cm_model_t *idCollisionModelManagerLocal::CollisionModelForBSPSubModel( int index, dmodel_t *m, dbrush_t *brushes, 
																	   dbrushside_t *brushSides, dplane_t *planes, drawVert_t *verts, dsurface_t *surfs, int *indexes) {
#if 1
	int i, j;
	idFixedWinding w;
	cm_node_t *node;
	cm_model_t *model;
	idPlane plane;
	idBounds bounds;
	bool collisionSurface;
	dsurface_t *surf;
	idMaterial *surfShader = this->declManager_FindMaterial("defaultShader");

	model = AllocModel();
	model->name = va("%i",index);
	node = AllocNode( model, NODE_BLOCK_SIZE_SMALL );
	node->planeType = -1;
	model->node = node;

	model->maxVertices = 0;
	model->numVertices = 0;
	model->maxEdges = 0;
	model->numEdges = 0;

	bounds.Clear();
	bounds.AddPoint(m->maxs);
	bounds.AddPoint(m->mins);

	for ( i = 0; i < m->numSurfaces; i++ ) {
		surf = &surfs[m->firstSurface+i];
		// if this surface has no contents
		//if ( ! ( surf->shader->GetContentFlags() & CONTENTS_REMOVE_UTIL ) ) {
		//	continue;
		//}
		// if the model has a collision surface and this surface is not a collision surface
		//if ( collisionSurface && !( surf->shader->GetSurfaceFlags() & SURF_COLLISION ) ) {
		//	continue;
		//}
		if(surf->surfaceType != MST_PLANAR)
			continue;
		// get max verts and edges
		model->maxVertices += surf->numVerts;
		model->maxEdges += surf->numIndexes;
	}

	if(model->maxVertices == 0 || model->maxEdges == 0) {
		return 0;
	}

	model->vertices = (cm_vertex_t *) Mem_ClearedAlloc( model->maxVertices * sizeof(cm_vertex_t) );
	model->edges = (cm_edge_t *) Mem_ClearedAlloc( model->maxEdges * sizeof(cm_edge_t) );

	// setup hash to speed up finding shared vertices and edges
	SetupHash();

	cm_vertexHash->ResizeIndex( model->maxVertices );
	cm_edgeHash->ResizeIndex( model->maxEdges );

	ClearHash( bounds );

	for ( i = 0; i < m->numSurfaces; i++ ) {
		surf = &surfs[m->firstSurface+i];
		// if this surface has no contents
		//if ( ! ( surf->shader->GetContentFlags() & CONTENTS_REMOVE_UTIL ) ) {
		//	continue;
		//}
		//// if the model has a collision surface and this surface is not a collision surface
		//if ( collisionSurface && !( surf->shader->GetSurfaceFlags() & SURF_COLLISION ) ) {
		//	continue;
		//}
		if(surf->surfaceType != MST_PLANAR)
			continue;
		for ( j = 0; j < surf->numIndexes; j += 3 ) {
			w.Clear();
#if 1
			w += verts[ surf->firstVert + indexes[ surf->firstIndex + j + 2 ] ].xyz;
			w += verts[ surf->firstVert + indexes[ surf->firstIndex + j + 1 ] ].xyz;
			w += verts[ surf->firstVert + indexes[ surf->firstIndex + j + 0 ] ].xyz;
#else
			w += verts[ surf->firstVert + indexes[ surf->firstIndex + j + 0 ] ].xyz;
			w += verts[ surf->firstVert + indexes[ surf->firstIndex + j + 1 ] ].xyz;
			w += verts[ surf->firstVert + indexes[ surf->firstIndex + j + 2 ] ].xyz;
#endif
			w.GetPlane( plane );
			plane = -plane;
			PolygonFromWinding( model, &w, plane, surfShader, 1 );
		}
	}

	// create a BSP tree for the model
	model->node = CreateAxialBSPTree( model, model->node );

	model->isConvex = false;

	FinishModel( model );

	// shutdown the hash
	ShutdownHash();

	return model;
#endif

}
void idCollisionModelManagerLocal::BuildModelsBSP( const char *fname ) {
	idTimer timer;
	timer.Start();

	if ( 1 || !LoadCollisionModelFile( fname, 0 ) ) {
		dheader_t *h;
		
		dplane_t *planes = (dplane_t *)(((byte*)h) + h->lumps[LUMP_PLANES].fileofs);
		dnode_t *nodes = (dnode_t *)(((byte*)h) + h->lumps[LUMP_NODES].fileofs);
		int numNodes = h->lumps[LUMP_NODES].filelen / sizeof(dnode_t);
		// load the nodes from bsp for data optimisation
		ParseBSPNodes(nodes,numNodes,planes);

		int numSubModels = h->lumps[LUMP_MODELS].filelen / sizeof(dmodel_t);		
		dmodel_t *models = (dmodel_t *)(((byte*)h) + h->lumps[LUMP_MODELS].fileofs);
		dbrush_t *brushes = (dbrush_t *)(((byte*)h) + h->lumps[LUMP_BRUSHES].fileofs);
		dbrushside_t *brushSides = (dbrushside_t *)(((byte*)h) + h->lumps[LUMP_BRUSHSIDES].fileofs);
		drawVert_t *verts = (drawVert_t *)(((byte*)h) + h->lumps[LUMP_DRAWVERTS].fileofs);
		dsurface_t *surfs = (dsurface_t *)(((byte*)h) + h->lumps[LUMP_SURFACES].fileofs);
		int *indexes = (int *)(((byte*)h) + h->lumps[LUMP_DRAWINDEXES].fileofs);
	
		///convert brushes and patches to collision data
		for ( int i = 0; i < numSubModels; i++ ) {
			dmodel_t *in = models + i;

			if ( numModels >= MAX_SUBMODELS ) {
				Com_FatalError( "idCollisionModelManagerLocal::BuildModels: more than %d collision models", MAX_SUBMODELS );
				break;
			}
			this->models[numModels] = CollisionModelForBSPSubModel( i, in, brushes, brushSides, planes, verts, surfs, indexes );
			if ( this->models[numModels] == 0) {
				//Com_FatalError("CollisionModelForBSPSubModel faield\n");
			}
			numModels++;
		}

		// free the proc bsp which is only used for data optimization
		Mem_Free( procNodes );
		procNodes = NULL;

		// write the collision models to a file
		WriteCollisionModelsToFile( fname, 0, numModels, 0 );
	}

	timer.Stop();

	// print statistics on collision data
	cm_model_t model;
	AccumulateModelInfo( &model );
	Com_Printf( "collision data:\n" );
	Com_Printf( "%6i models\n", numModels );
	PrintModelInfo( &model );
	Com_Printf( "%.0f msec to load collision data.\n", timer.Milliseconds() );
}

