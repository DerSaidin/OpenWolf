/*
===========================================================================
Copyright (C) 2012 su44

This file is part of OpenWolf source code.

OpenWolf source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenWolf source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenWolf source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "../cm/CollisionModel_local.h"

#include "cm_local.h"

idCollisionModelManagerLocal newCM;

#define USE_OLD_CM

extern "C" {
	void CM_LoadMapOLD( const char *name, bool clientload, int *checksum );
	int CM_PointContentsOLD( const vec3_t p, clipHandle_t model );
	int CM_TransformedPointContentsOLD( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles );
	void CM_BoxTraceOLD(trace_t * results, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, clipHandle_t model, int brushmask, traceType_t type);
	void CM_TransformedBoxTraceOLD(trace_t * results, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles, traceType_t type);
}

#ifdef USE_OLD_CM
bool cm_useNew = qfalse;
#else
bool cm_useNew = qtrue;
#endif
void CM_LoadMap( const char *name, qboolean clientload, int *checksum ) {

	if(cm_useNew) {
		// load strictly collision detection related data from .cm file (or generate it)
		idMath::Init();
#ifndef USE_OLD_CM
		newCM.LoadMap(name,clientload,checksum);
#endif
	} else {
		// load leafs, nodes, pvs, etc,
		// used eg by serverside culling (SV_AddEntitiesVisibleFromPoint)
		CM_LoadMapOLD(name,clientload,checksum);
	}

}

int CM_PointContents( const vec3_t p, clipHandle_t model ) {
	int r;
	if(cm_useNew) {	
		r = newCM.PointContents(p,model);
	} else {
		r = CM_PointContentsOLD(p,model);
	}
	return r;
}

int CM_TransformedPointContents( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles ) {
	int r;
	if(cm_useNew) {
		r = newCM.TransformedPointContents(p,model,origin,angles);
	} else {
		r = CM_TransformedPointContentsOLD(p,model,origin,angles);
	}
	return r;
}

void CM_BoxTrace(trace_t * results, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, clipHandle_t model, int brushmask, traceType_t type) {
	if(cm_useNew) {
		idTraceModel trm;
		idBounds bb;
		if(mins != 0 && maxs != 0) {
			bb.AddPoint(mins);
			bb.AddPoint(maxs);
			trm.SetupBox(bb);
		} else {
			// isnt there a way to setup a POINT trm ?
			trm.SetupBox(1.f);
		}
	
		memset(results,0,sizeof(trace_t));

		d3trace_t tr;
		newCM.Translation(&tr,start,end,&trm,mat3_identity,brushmask,model,vec3_origin,mat3_identity);

		if(tr.c.type == CONTACT_NONE) {
			results->fraction = 1.f;
			VectorCopy(end,results->endpos);
			results->entityNum = ENTITYNUM_NONE;
		} else {
			results->fraction = tr.fraction;
			VectorCopy(tr.endpos,results->endpos);
			results->contents = tr.c.contents;
			results->plane.dist = -tr.c.dist;
			VectorCopy(tr.c.normal,results->plane.normal);
			results->entityNum = ENTITYNUM_WORLD;
			if(results->fraction == 0.f) {
				results->allsolid = qtrue;
				results->startsolid = qtrue;
			}
		}
	} else {
		return CM_BoxTraceOLD(results,start,end,mins,maxs,model,brushmask,type);
	}
}
void CM_TransformedBoxTrace(trace_t * results, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles, traceType_t type) {
	if(cm_useNew) {
		idTraceModel trm;
		idBounds bb;
		if(mins != 0 && maxs != 0) {
			bb.AddPoint(mins);
			bb.AddPoint(maxs);
			trm.SetupBox(bb);
		} else {
			// isnt there a way to setup a POINT trm ?
			trm.SetupBox(1.f);
		}

		memset(results,0,sizeof(trace_t));

		d3trace_t tr;
		idMat3 modelAxis;
		AnglesToAxis(angles,modelAxis.getAxis());
		newCM.Translation(&tr,start,end,&trm,mat3_identity,brushmask,model,origin,modelAxis);

		if(tr.c.type == CONTACT_NONE) {
			results->fraction = 1.f;
			VectorCopy(end,results->endpos);
			results->entityNum = ENTITYNUM_NONE;
		} else {
			results->fraction = tr.fraction;
			VectorCopy(tr.endpos,results->endpos);
			results->contents = tr.c.contents;
			results->plane.dist = -tr.c.dist;
			VectorCopy(tr.c.normal,results->plane.normal);
			results->entityNum = ENTITYNUM_WORLD;
			if(results->fraction == 0.f) {
				results->allsolid = qtrue;
				results->startsolid = qtrue;
			}
		}
	} else {
		return CM_TransformedBoxTraceOLD(results, start, end, mins, maxs, model, brushmask, origin, angles, type);
	}
}

