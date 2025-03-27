/******************************************************************************/
/*!
\file		Collision.cpp
\author 	Liu YaoTing
\par    	email: yaoting.liu@digipen.edu
\date   	Feb 09, 2024
\brief		ToDo:This file provides definition for function CollisionIntersection_RectRect.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#include "main.h"

/**************************************************************************/
/*!

	*/
/**************************************************************************/
bool CollisionIntersection_RectRect(const AABB & aabb1,          //Input
									const AEVec2 & vel1,         //Input 
									const AABB & aabb2,          //Input 
									const AEVec2 & vel2,         //Input
									float& firstTimeOfCollision) //Output: the calculated value of tFirst, below, must be returned here
{
	UNREFERENCED_PARAMETER(aabb1);
	UNREFERENCED_PARAMETER(vel1);
	UNREFERENCED_PARAMETER(aabb2);
	UNREFERENCED_PARAMETER(vel2);
	UNREFERENCED_PARAMETER(firstTimeOfCollision);

	/*
	Implement the collision intersection over here.

	The steps are:	
	Step 1: Check for static collision detection between rectangles (static: before moving). 
				If the check returns no overlap, you continue with the dynamic collision test
					with the following next steps 2 to 5 (dynamic: with velocities).
				Otherwise you return collision is true, and you stop.

	Step 2: Initialize and calculate the new velocity of Vb
			tFirst = 0  //tFirst variable is commonly used for both the x-axis and y-axis
			tLast = dt  //tLast variable is commonly used for both the x-axis and y-axis

	Step 3: Working with one dimension (x-axis).
			if(Vb < 0)
				case 1
				case 4
			else if(Vb > 0)
				case 2
				case 3
			else //(Vb == 0)
				case 5

			case 6

	Step 4: Repeat step 3 on the y-axis

	Step 5: Return true: the rectangles intersect

	*/
	// step 1
	if (aabb1.max.x < aabb2.min.x || aabb1.max.y < aabb2.min.y ||
		aabb1.min.x > aabb2.max.x || aabb1.min.y > aabb2.max.y) {
		// step 2
		AEVec2 v_rel{};
		v_rel.x = vel2.x - vel1.x;
		v_rel.y = vel2.y - vel1.y;
		double tFirst = 0.0f;
		double tLast = g_dt;

		// step 3
		if (v_rel.x < 0) {
			if (aabb1.min.x > aabb2.max.x) {// case 1
				return 0;
			}
			if (aabb1.max.x < aabb2.min.x) {// case 4
				tFirst = fmax((aabb1.max.x - aabb2.min.x) / v_rel.x, tFirst);
				
			}
			if (aabb1.min.x < aabb2.max.x) {
				tLast = fmin((aabb1.min.x - aabb2.max.x) / v_rel.x, tLast);
			}
		}
		else if (v_rel.x > 0) {
			if (aabb1.min.x > aabb2.max.x) {// case 2
				tFirst = fmax((aabb1.min.x - aabb2.max.x) / v_rel.x, tFirst);
				
			}
			if (aabb1.max.x > aabb2.min.x) {
				tLast = fmin((aabb1.max.x - aabb2.min.x) / v_rel.x, tLast);
			}
			if (aabb1.max.x < aabb2.min.x) {//  case 3
				return 0;
			}
		}
		else {// case 5
			if (aabb1.max.x < aabb2.min.x) {
				return 0;
			}
			else if (aabb1.min.x > aabb2.max.x) {
				return 0;
			}
		}
		if (tFirst > tLast) // case 6
			return 0;
		// step 4
		if (v_rel.y < 0) {
			if (aabb1.min.y > aabb2.max.y) {// case 1
				return 0;
			}
			if (aabb1.max.y < aabb2.min.y) {// case 4
				tFirst = fmax((aabb1.max.y - aabb2.min.y) / v_rel.y, tFirst);
				
			}
			if (aabb1.min.y < aabb2.max.y) {
				tLast = fmin((aabb1.min.y - aabb2.max.y) / v_rel.y, tLast);
			}
		}
		else if (v_rel.y > 0) {
			if (aabb1.min.y > aabb2.max.y) {// case 2
				tFirst = fmax((aabb1.min.y - aabb2.max.y) / v_rel.y, tFirst);
				
			}
			if (aabb1.max.y > aabb2.min.y) {
				tLast = fmin((aabb1.max.y - aabb2.min.y) / v_rel.y, tLast);
			}
			if (aabb1.max.y < aabb2.min.y) {//  case 3
				return 0;
			}
		}
		else {// case 5
			if (aabb1.max.y < aabb2.min.y) {
				return 0;
			}
			else if (aabb1.min.y > aabb2.max.y) {
				return 0;
			}
		}
		if (tFirst > tLast)//case 6
			return 0;
		return 1;
	}
	else {
		firstTimeOfCollision = 0.0f;
		return 1;
	}
	
}