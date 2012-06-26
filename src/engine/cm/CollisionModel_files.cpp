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

/*
===============================================================================

	Trace model vs. polygonal model collision detection.

===============================================================================
*/

#include "../idLib/precompiled.h"
#pragma hdrstop

#include "CollisionModel_local.h"

#define CM_FILE_EXT			"cm"
#define CM_FILEID			"CM"
#define CM_FILEVERSION		"1.00"


/*
===============================================================================

Writing of collision model file

===============================================================================
*/

void CM_GetNodeBounds( idBounds *bounds, cm_node_t *node );
int CM_GetNodeContents( cm_node_t *node );


/*
================
idCollisionModelManagerLocal::WriteNodes
================
*/
void idCollisionModelManagerLocal::WriteNodes( fileHandle_t fp, cm_node_t *node ) {
	FS_FPrintf(fp, "\t( %d %f )\n", node->planeType, node->planeDist );
	if ( node->planeType != -1 ) {
		WriteNodes( fp, node->children[0] );
		WriteNodes( fp, node->children[1] );
	}
}

/*
================
idCollisionModelManagerLocal::CountPolygonMemory
================
*/
int idCollisionModelManagerLocal::CountPolygonMemory( cm_node_t *node ) const {
	cm_polygonRef_t *pref;
	cm_polygon_t *p;
	int memory;

	memory = 0;
	for ( pref = node->polygons; pref; pref = pref->next ) {
		p = pref->p;
		if ( p->checkcount == checkCount ) {
			continue;
		}
		p->checkcount = checkCount;

		memory += sizeof( cm_polygon_t ) + ( p->numEdges - 1 ) * sizeof( p->edges[0] );
	}
	if ( node->planeType != -1 ) {
		memory += CountPolygonMemory( node->children[0] );
		memory += CountPolygonMemory( node->children[1] );
	}
	return memory;
}

/*
================
idCollisionModelManagerLocal::WritePolygons
================
*/
void idCollisionModelManagerLocal::WritePolygons( fileHandle_t fp, cm_node_t *node ) {
	cm_polygonRef_t *pref;
	cm_polygon_t *p;
	int i;

	for ( pref = node->polygons; pref; pref = pref->next ) {
		p = pref->p;
		if ( p->checkcount == checkCount ) {
			continue;
		}
		p->checkcount = checkCount;
		FS_FPrintf(fp, "\t%d (", p->numEdges );
		for ( i = 0; i < p->numEdges; i++ ) {
			FS_FPrintf(fp, " %d", p->edges[i] );
		}
		FS_FPrintf(fp, " ) ( %f %f %f ) %f", p->plane.Normal()[0], p->plane.Normal()[1], p->plane.Normal()[2], p->plane.Dist() );
		FS_FPrintf(fp, " ( %f %f %f )", p->bounds[0][0], p->bounds[0][1], p->bounds[0][2] );
		FS_FPrintf(fp, " ( %f %f %f )", p->bounds[1][0], p->bounds[1][1], p->bounds[1][2] );
		FS_FPrintf(fp, " \"%s\"\n", p->material->GetName() );
	}
	if ( node->planeType != -1 ) {
		WritePolygons( fp, node->children[0] );
		WritePolygons( fp, node->children[1] );
	}
}

/*
================
idCollisionModelManagerLocal::CountBrushMemory
================
*/
int idCollisionModelManagerLocal::CountBrushMemory( cm_node_t *node ) const {
	cm_brushRef_t *bref;
	cm_brush_t *b;
	int memory;

	memory = 0;
	for ( bref = node->brushes; bref; bref = bref->next ) {
		b = bref->b;
		if ( b->checkcount == checkCount ) {
			continue;
		}
		b->checkcount = checkCount;

		memory += sizeof( cm_brush_t ) + ( b->numPlanes - 1 ) * sizeof( b->planes[0] );
	}
	if ( node->planeType != -1 ) {
		memory += CountBrushMemory( node->children[0] );
		memory += CountBrushMemory( node->children[1] );
	}
	return memory;
}

/*
================
idCollisionModelManagerLocal::WriteBrushes
================
*/
void idCollisionModelManagerLocal::WriteBrushes( fileHandle_t fp, cm_node_t *node ) {
	cm_brushRef_t *bref;
	cm_brush_t *b;
	int i;

	for ( bref = node->brushes; bref; bref = bref->next ) {
		b = bref->b;
		if ( b->checkcount == checkCount ) {
			continue;
		}
		b->checkcount = checkCount;
		FS_FPrintf(fp, "\t%d {\n", b->numPlanes );
		for ( i = 0; i < b->numPlanes; i++ ) {
			FS_FPrintf(fp, "\t\t( %f %f %f ) %f\n", b->planes[i].Normal()[0], b->planes[i].Normal()[1], b->planes[i].Normal()[2], b->planes[i].Dist() );
		}
		FS_FPrintf(fp, "\t} ( %f %f %f )", b->bounds[0][0], b->bounds[0][1], b->bounds[0][2] );
		FS_FPrintf(fp, " ( %f %f %f ) \"%s\"\n", b->bounds[1][0], b->bounds[1][1], b->bounds[1][2], StringFromContents( b->contents ) );
	}
	if ( node->planeType != -1 ) {
		WriteBrushes( fp, node->children[0] );
		WriteBrushes( fp, node->children[1] );
	}
}

/*
================
idCollisionModelManagerLocal::WriteCollisionModel
================
*/
void idCollisionModelManagerLocal::WriteCollisionModel( fileHandle_t fp, cm_model_t *model ) {
	int i, polygonMemory, brushMemory;

	if(model == 0)
		return;

	FS_FPrintf(fp, "collisionModel \"%s\" {\n", model->name.c_str() );
	// vertices
	FS_FPrintf(fp, "\tvertices { /* numVertices = */ %d\n", model->numVertices );
	for ( i = 0; i < model->numVertices; i++ ) {
		FS_FPrintf(fp, "\t/* %d */ ( %f %f %f )\n", i, model->vertices[i].p[0], model->vertices[i].p[1], model->vertices[i].p[2] );
	}
	FS_FPrintf(fp, "\t}\n" );
	// edges
	FS_FPrintf(fp, "\tedges { /* numEdges = */ %d\n", model->numEdges );
	for ( i = 0; i < model->numEdges; i++ ) {
		FS_FPrintf(fp, "\t/* %d */ ( %d %d ) %d %d\n", i, model->edges[i].vertexNum[0], model->edges[i].vertexNum[1], model->edges[i].internal, model->edges[i].numUsers );
	}
	FS_FPrintf(fp, "\t}\n" );
	// nodes
	FS_FPrintf(fp, "\tnodes {\n" );
	WriteNodes( fp, model->node );
	FS_FPrintf(fp, "\t}\n" );
	// polygons
	checkCount++;
	polygonMemory = CountPolygonMemory( model->node );
	FS_FPrintf(fp, "\tpolygons /* polygonMemory = */ %d {\n", polygonMemory );
	checkCount++;
	WritePolygons( fp, model->node );
	FS_FPrintf(fp, "\t}\n" );
	// brushes
	checkCount++;
	brushMemory = CountBrushMemory( model->node );
	FS_FPrintf(fp, "\tbrushes /* brushMemory = */ %d {\n", brushMemory );
	checkCount++;
	WriteBrushes( fp, model->node );
	FS_FPrintf(fp, "\t}\n" );
	// closing brace
	FS_FPrintf(fp, "}\n" );
}

/*
================
idCollisionModelManagerLocal::WriteCollisionModelsToFile
================
*/
void idCollisionModelManagerLocal::WriteCollisionModelsToFile( const char *filename, int firstModel, int lastModel, unsigned int mapFileCRC ) {
	int i;
	fileHandle_t fp;
	idStr name;
	
	name = filename;
	name.SetFileExtension( CM_FILE_EXT );

	Com_Printf( "writing %s\n", name.c_str() );
	// _D3XP was saving to fs_cdpath
	fp = FS_FOpenFileWrite( name );
	if ( !fp ) {
		Com_Warning( "idCollisionModelManagerLocal::WriteCollisionModelsToFile: Error opening file %s\n", name.c_str() );
		return;
	}

	// write file id and version
	FS_FPrintf(fp, "%s \"%s\"\n\n", CM_FILEID, CM_FILEVERSION );
	// write the map file crc
	FS_FPrintf(fp, "%u\n\n", mapFileCRC );

	// write the collision models
	for ( i = firstModel; i < lastModel; i++ ) {
		WriteCollisionModel( fp, models[ i ] );
	}

	FS_FCloseFile( fp );
}

/*
================
idCollisionModelManagerLocal::WriteCollisionModelForMapEntity
================
*/
bool idCollisionModelManagerLocal::WriteCollisionModelForMapEntity( const idMapEntity *mapEnt, const char *filename, const bool testTraceModel ) {
	fileHandle_t fp;
	idStr name;
	cm_model_t *model;

	SetupHash();
	model = CollisionModelForMapEntity( mapEnt );
	model->name = filename;

	name = filename;
	name.SetFileExtension( CM_FILE_EXT );

	Com_Printf( "writing %s\n", name.c_str() );
	fp = FS_FOpenFileWrite( name );
	if ( !fp ) {
		Com_Printf( "idCollisionModelManagerLocal::WriteCollisionModelForMapEntity: Error opening file %s\n", name.c_str() );
		FreeModel( model );
		return false;
	}

	// write file id and version
	FS_FPrintf(fp, "%s \"%s\"\n\n", CM_FILEID, CM_FILEVERSION );
	// write the map file crc
	FS_FPrintf(fp, "%u\n\n", 0 );

	// write the collision model
	WriteCollisionModel( fp, model );

	FS_FCloseFile( fp );

	if ( testTraceModel ) {
		idTraceModel trm;
		TrmFromModel( model, trm );
	}

	FreeModel( model );

	return true;
}


/*
===============================================================================

Loading of collision model file

===============================================================================
*/

/*
================
idCollisionModelManagerLocal::ParseVertices
================
*/
void idCollisionModelManagerLocal::ParseVertices( idLexer *src, cm_model_t *model ) {
	int i;

	src->ExpectTokenString( "{" );
	model->numVertices = src->ParseInt();
	model->maxVertices = model->numVertices;
	model->vertices = (cm_vertex_t *) Mem_Alloc( model->maxVertices * sizeof( cm_vertex_t ) );
	for ( i = 0; i < model->numVertices; i++ ) {
		src->Parse1DMatrix( 3, model->vertices[i].p.ToFloatPtr() );
		model->vertices[i].side = 0;
		model->vertices[i].sideSet = 0;
		model->vertices[i].checkcount = 0;
	}
	src->ExpectTokenString( "}" );
}

/*
================
idCollisionModelManagerLocal::ParseEdges
================
*/
void idCollisionModelManagerLocal::ParseEdges( idLexer *src, cm_model_t *model ) {
	int i;

	src->ExpectTokenString( "{" );
	model->numEdges = src->ParseInt();
	model->maxEdges = model->numEdges;
	model->edges = (cm_edge_t *) Mem_Alloc( model->maxEdges * sizeof( cm_edge_t ) );
	for ( i = 0; i < model->numEdges; i++ ) {
		src->ExpectTokenString( "(" );
		model->edges[i].vertexNum[0] = src->ParseInt();
		model->edges[i].vertexNum[1] = src->ParseInt();
		src->ExpectTokenString( ")" );
		model->edges[i].side = 0;
		model->edges[i].sideSet = 0;
		model->edges[i].internal = src->ParseInt();
		model->edges[i].numUsers = src->ParseInt();
		model->edges[i].normal = vec3_origin;
		model->edges[i].checkcount = 0;
		model->numInternalEdges += model->edges[i].internal;
	}
	src->ExpectTokenString( "}" );
}

/*
================
idCollisionModelManagerLocal::ParseNodes
================
*/
cm_node_t *idCollisionModelManagerLocal::ParseNodes( idLexer *src, cm_model_t *model, cm_node_t *parent ) {
	cm_node_t *node;

	model->numNodes++;
	node = AllocNode( model, model->numNodes < NODE_BLOCK_SIZE_SMALL ? NODE_BLOCK_SIZE_SMALL : NODE_BLOCK_SIZE_LARGE );
	node->brushes = NULL;
	node->polygons = NULL;
	node->parent = parent;
	src->ExpectTokenString( "(" );
	node->planeType = src->ParseInt();
	node->planeDist = src->ParseFloat();
	src->ExpectTokenString( ")" );
	if ( node->planeType != -1 ) {
		node->children[0] = ParseNodes( src, model, node );
		node->children[1] = ParseNodes( src, model, node );
	}
	return node;
}

/*
================
idCollisionModelManagerLocal::ParsePolygons
================
*/
void idCollisionModelManagerLocal::ParsePolygons( idLexer *src, cm_model_t *model ) {
	cm_polygon_t *p;
	int i, numEdges;
	idVec3 normal;
	idToken token;

	if ( src->CheckTokenType( TT_NUMBER, 0, &token ) ) {
		model->polygonBlock = (cm_polygonBlock_t *) Mem_Alloc( sizeof( cm_polygonBlock_t ) + token.GetIntValue() );
		model->polygonBlock->bytesRemaining = token.GetIntValue();
		model->polygonBlock->next = ( (byte *) model->polygonBlock ) + sizeof( cm_polygonBlock_t );
	}

	src->ExpectTokenString( "{" );
	while ( !src->CheckTokenString( "}" ) ) {
		// parse polygon
		numEdges = src->ParseInt();
		p = AllocPolygon( model, numEdges );
		p->numEdges = numEdges;
		src->ExpectTokenString( "(" );
		for ( i = 0; i < p->numEdges; i++ ) {
			p->edges[i] = src->ParseInt();
		}
		src->ExpectTokenString( ")" );
		src->Parse1DMatrix( 3, normal.ToFloatPtr() );
		p->plane.SetNormal( normal );
		p->plane.SetDist( src->ParseFloat() );
		src->Parse1DMatrix( 3, p->bounds[0].ToFloatPtr() );
		src->Parse1DMatrix( 3, p->bounds[1].ToFloatPtr() );
		src->ExpectTokenType( TT_STRING, 0, &token );
		// get material
		p->material = declManager_FindMaterial( token );
		p->contents = p->material->GetContentFlags();
		p->checkcount = 0;
		// filter polygon into tree
		R_FilterPolygonIntoTree( model, model->node, NULL, p );
	}
}

/*
================
idCollisionModelManagerLocal::ParseBrushes
================
*/
void idCollisionModelManagerLocal::ParseBrushes( idLexer *src, cm_model_t *model ) {
	cm_brush_t *b;
	int i, numPlanes;
	idVec3 normal;
	idToken token;

	if ( src->CheckTokenType( TT_NUMBER, 0, &token ) ) {
		model->brushBlock = (cm_brushBlock_t *) Mem_Alloc( sizeof( cm_brushBlock_t ) + token.GetIntValue() );
		model->brushBlock->bytesRemaining = token.GetIntValue();
		model->brushBlock->next = ( (byte *) model->brushBlock ) + sizeof( cm_brushBlock_t );
	}

	src->ExpectTokenString( "{" );
	while ( !src->CheckTokenString( "}" ) ) {
		// parse brush
		numPlanes = src->ParseInt();
		b = AllocBrush( model, numPlanes );
		b->numPlanes = numPlanes;
		src->ExpectTokenString( "{" );
		for ( i = 0; i < b->numPlanes; i++ ) {
			src->Parse1DMatrix( 3, normal.ToFloatPtr() );
			b->planes[i].SetNormal( normal );
			b->planes[i].SetDist( src->ParseFloat() );
		}
		src->ExpectTokenString( "}" );
		src->Parse1DMatrix( 3, b->bounds[0].ToFloatPtr() );
		src->Parse1DMatrix( 3, b->bounds[1].ToFloatPtr() );
		src->ReadToken( &token );
		if ( token.type == TT_NUMBER ) {
			b->contents = token.GetIntValue();		// old .cm files use a single integer
		} else {
			b->contents = ContentsFromString( token );
		}
		b->checkcount = 0;
		b->primitiveNum = 0;
		// filter brush into tree
		R_FilterBrushIntoTree( model, model->node, NULL, b );
	}
}

/*
================
idCollisionModelManagerLocal::ParseCollisionModel
================
*/
bool idCollisionModelManagerLocal::ParseCollisionModel( idLexer *src ) {
	cm_model_t *model;
	idToken token;

	if ( numModels >= MAX_SUBMODELS ) {
		Com_FatalError( "LoadModel: no free slots" );
		return false;
	}
	model = AllocModel();
	models[numModels ] = model;
	numModels++;
	// parse the file
	src->ExpectTokenType( TT_STRING, 0, &token );
	model->name = token;
	src->ExpectTokenString( "{" );
	while ( !src->CheckTokenString( "}" ) ) {

		src->ReadToken( &token );

		if ( token == "vertices" ) {
			ParseVertices( src, model );
			continue;
		}

		if ( token == "edges" ) {
			ParseEdges( src, model );
			continue;
		}

		if ( token == "nodes" ) {
			src->ExpectTokenString( "{" );
			model->node = ParseNodes( src, model, NULL );
			src->ExpectTokenString( "}" );
			continue;
		}

		if ( token == "polygons" ) {
			ParsePolygons( src, model );
			continue;
		}

		if ( token == "brushes" ) {
			ParseBrushes( src, model );
			continue;
		}

		src->Error( "ParseCollisionModel: bad token \"%s\"", token.c_str() );
	}
	// calculate edge normals
	checkCount++;
	CalculateEdgeNormals( model, model->node );
	// get model bounds from brush and polygon bounds
	CM_GetNodeBounds( &model->bounds, model->node );
	// get model contents
	model->contents = CM_GetNodeContents( model->node );
	// total memory used by this model
	model->usedMemory = model->numVertices * sizeof(cm_vertex_t) +
						model->numEdges * sizeof(cm_edge_t) +
						model->polygonMemory +
						model->brushMemory +
						model->numNodes * sizeof(cm_node_t) +
						model->numPolygonRefs * sizeof(cm_polygonRef_t) +
						model->numBrushRefs * sizeof(cm_brushRef_t);

	return true;
}

/*
================
idCollisionModelManagerLocal::LoadCollisionModelFile
================
*/
bool idCollisionModelManagerLocal::LoadCollisionModelFile( const char *name, unsigned int mapFileCRC ) {
	idStr fileName;
	idToken token;
	idLexer *src;
	unsigned int crc;

	// load it
	fileName = name;
	fileName.SetFileExtension( CM_FILE_EXT );
	src = new idLexer( fileName );
	src->SetFlags( LEXFL_NOSTRINGCONCAT | LEXFL_NODOLLARPRECOMPILE );
	if ( !src->IsLoaded() ) {
		delete src;
		return false;
	}

	if ( !src->ExpectTokenString( CM_FILEID ) ) {
		Com_Warning( "%s is not an CM file.", fileName.c_str() );
		delete src;
		return false;
	}

	if ( !src->ReadToken( &token ) || token != CM_FILEVERSION ) {
		Com_Warning( "%s has version %s instead of %s", fileName.c_str(), token.c_str(), CM_FILEVERSION );
		delete src;
		return false;
	}

	if ( !src->ExpectTokenType( TT_NUMBER, TT_INTEGER, &token ) ) {
		Com_Warning( "%s has no map file CRC", fileName.c_str() );
		delete src;
		return false;
	}

	crc = token.GetUnsignedLongValue();
	if ( mapFileCRC && crc != mapFileCRC ) {
		Com_Printf( "%s is out of date\n", fileName.c_str() );
		delete src;
		return false;
	}

	// parse the file
	while ( 1 ) {
		if ( !src->ReadToken( &token ) ) {
			break;
		}

		if ( token == "collisionModel" ) {
			if ( !ParseCollisionModel( src ) ) {
				delete src;
				return false;
			}
			continue;
		}

		src->Error( "idCollisionModelManagerLocal::LoadCollisionModelFile: bad token \"%s\"", token.c_str() );
	}

	delete src;

	return true;
}
