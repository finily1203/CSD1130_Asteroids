/******************************************************************************/
/*!
\file		GameState_Asteroids.cpp
\author 	Liu YaoTing
\par    	email: yaoting.liu@digipen.edu
\date   	Feb 09, 2024
\brief		ToDo:This file provides definition for function gameasteroid load, init, update, draw, free, unload.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "main.h"
#include <iostream>
#include <string> 
/******************************************************************************/
/*!
	Defines
*/
/******************************************************************************/
const unsigned int	GAME_OBJ_NUM_MAX		= 32;			// The total number of different objects (Shapes)
const unsigned int	GAME_OBJ_INST_NUM_MAX	= 2048;			// The total number of different game object instances


const unsigned int	SHIP_INITIAL_NUM		= 3;			// initial number of ship lives
const float			SHIP_SCALE_X			= 16.0f;		// ship scale x
const float			SHIP_SCALE_Y			= 16.0f;		// ship scale y
const float			BULLET_SCALE_X			= 20.0f;		// bullet scale x
const float			BULLET_SCALE_Y			= 3.0f;			// bullet scale y
const float			ASTEROID_MIN_SCALE_X	= 10.0f;		// asteroid minimum scale x
const float			ASTEROID_MAX_SCALE_X	= 60.0f;		// asteroid maximum scale x
const float			ASTEROID_MIN_SCALE_Y	= 10.0f;		// asteroid minimum scale y
const float			ASTEROID_MAX_SCALE_Y	= 60.0f;		// asteroid maximum scale y

const float			WALL_SCALE_X			= 64.0f;		// wall scale x
const float			WALL_SCALE_Y			= 164.0f;		// wall scale y

const float			SHIP_ACCEL_FORWARD		= 100.0f;		// ship forward acceleration (in m/s^2)
const float			SHIP_ACCEL_BACKWARD		= 100.0f;		// ship backward acceleration (in m/s^2)
const float			SHIP_ROT_SPEED			= (2.0f * PI);	// ship rotation speed (degree/second)

const float			BULLET_SPEED			= 400.0f;		// bullet speed (m/s)

const float         BOUNDING_RECT_SIZE      = 1.0f;         // this is the normalized bounding rectangle (width and height) sizes - AABB collision data

// Additional texture and font
s8 pFont;
AEGfxTexture* pTex, *pTex1, *pTexship;
AEGfxVertexList* pMesh = 0;
// -----------------------------------------------------------------------------
enum TYPE
{
	// list of game object types
	TYPE_SHIP = 0, 
	TYPE_BULLET,
	TYPE_ASTEROID,
	TYPE_WALL,

	TYPE_NUM
};

// -----------------------------------------------------------------------------
// object flag definition

const unsigned long FLAG_ACTIVE				= 0x00000001;
static bool onValueChange = true;
static bool over = false;
/******************************************************************************/
/*!
	Struct/Class Definitions
*/
/******************************************************************************/

//Game object structure
struct GameObj
{
	unsigned long		type;		// object type
	AEGfxVertexList *	pMesh;		// This will hold the triangles which will form the shape of the object
};

// ---------------------------------------------------------------------------

//Game object instance structure
struct GameObjInst
{
	GameObj *			pObject;	// pointer to the 'original' shape
	unsigned long		flag;		// bit flag or-ed together
	AEVec2				scale;		// scaling value of the object instance
	AEVec2				posCurr;	// object current position

	AEVec2				posPrev;	// object previous position -> it's the position calculated in the previous loop

	AEVec2				velCurr;	// object current velocity
	float				dirCurr;	// object current direction
	AABB				boundingBox;// object bouding box that encapsulates the object
	AEMtx33				transform;	// object transformation matrix: Each frame, 
									// calculate the object instance's transformation matrix and save it here

};

/******************************************************************************/
/*!
	Static Variables
*/
/******************************************************************************/

// list of original object
static GameObj				sGameObjList[GAME_OBJ_NUM_MAX];				// Each element in this array represents a unique game object (shape)
static unsigned long		sGameObjNum;								// The number of defined game objects

// list of object instances
static GameObjInst			sGameObjInstList[GAME_OBJ_INST_NUM_MAX];	// Each element in this array represents a unique game object instance (sprite)
static unsigned long		sGameObjInstNum;							// The number of used game object instances

// pointer to the ship object
static GameObjInst *		spShip;										// Pointer to the "Ship" game object instance

// pointer to the wall object
static GameObjInst *		spWall;										// Pointer to the "Wall" game object instance

// number of ship available (lives 0 = game over)
static long					sShipLives;									// The number of lives left

// the score = number of asteroid destroyed
static unsigned long		sScore;										// Current score

// ---------------------------------------------------------------------------

// functions to create/destroy a game object instance
GameObjInst *		gameObjInstCreate (unsigned long type, AEVec2* scale,
											   AEVec2 * pPos, AEVec2 * pVel, float dir);
void				gameObjInstDestroy(GameObjInst * pInst);

void				Helper_Wall_Collision();


/******************************************************************************/
/*!
	"Load" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsLoad(void)
{
	// load additional features
	pFont = AEGfxCreateFont("../Resources/Fonts/Arial_Italic.ttf", 72);
	pTex = AEGfxTextureLoad("../Resources/Textures/space_background.png");
	pTex1 = AEGfxTextureLoad("../Resources/Textures/as.png");
	pTexship = AEGfxTextureLoad("../Resources/Textures/ship.png");

	// zero the game object array
	memset(sGameObjList, 0, sizeof(GameObj) * GAME_OBJ_NUM_MAX);
	// No game objects (shapes) at this point
	sGameObjNum = 0;

	// zero the game object instance array
	memset(sGameObjInstList, 0, sizeof(GameObjInst) * GAME_OBJ_INST_NUM_MAX);
	// No game object instances (sprites) at this point
	sGameObjInstNum = 0;

	// The ship object instance hasn't been created yet, so this "spShip" pointer is initialized to 0
	spShip = nullptr;

	// load/create the mesh data (game objects / Shapes)
	GameObj* pObj;

	// =====================
	// create the ship shape
	// =====================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_SHIP;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 1.0f,  // bottom-left: red
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue

	AEGfxTriAdd(
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		0.5f, 0.5f, 0xFFFFFFFF, 1.0f, 0.0f,    // top-right: white
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	// =======================
	// create the bullet shape
	// =======================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_BULLET;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		-0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f);
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	// =========================
	// create the asteroid shape
	// =========================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_ASTEROID;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 1.0f,  // bottom-left: red
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue

	AEGfxTriAdd(
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		0.5f, 0.5f, 0xFFFFFFFF, 1.0f, 0.0f,    // top-right: white
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue


	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	// =========================
	// create the wall shape
	// =========================
	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_WALL;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		-0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f);
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");

	// =========================
	// create the background
	// =========================

	AEGfxMeshStart();

	// This shape has 2 triangles that makes up a square
	// Color parameters represent colours as ARGB
	// UV coordinates to read from loaded textures
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 1.0f,  // bottom-left: red
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue

	AEGfxTriAdd(
		0.5f, -0.5f, 0xFF00FF00, 1.0f, 1.0f,   // bottom-right: green
		0.5f, 0.5f, 0xFFFFFFFF, 1.0f, 0.0f,    // top-right: white
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f);  // top-left: blue

	// Saving the mesh (list of triangles) in pMesh
	pMesh = AEGfxMeshEnd();

}

/******************************************************************************/
/*!
	"Initialize" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsInit(void)
{

	// create the main ship
	AEVec2 scale;
	AEVec2Set(&scale, SHIP_SCALE_X * 2.5f, SHIP_SCALE_Y * 2.5f);
	spShip = gameObjInstCreate(TYPE_SHIP, &scale, nullptr, nullptr, 0.0f);
	AE_ASSERT(spShip);
	
	// create the initial 4 asteroids instances using the "gameObjInstCreate" function
	AEVec2 pos{}, vel{};

	//Asteroid 1
	pos.x = 90.0f;		pos.y = -220.0f;
	vel.x = -60.0f;		vel.y = -30.0f;
	AEVec2Set(&scale, ASTEROID_MAX_SCALE_X,
		ASTEROID_MAX_SCALE_X);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);


	//Asteroid 2
	pos.x = -260.0f;	pos.y = -250.0f;
	vel.x = 39.0f;		vel.y = -130.0f;
	AEVec2Set(&scale, ASTEROID_MAX_SCALE_X * 1.5,
		ASTEROID_MAX_SCALE_X * 1.5);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	//Asteroid 3
	pos.x = -50.0f;	pos.y = -280.0f;
	vel.x = 70.0f;		vel.y = 100.0f;
	AEVec2Set(&scale, ASTEROID_MAX_SCALE_X * 0.8,
		ASTEROID_MAX_SCALE_X * 0.8);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	//Asteroid 4
	pos.x = 100.0f;	pos.y = -300.0f;
	vel.x = -100.0f;		vel.y = 60.0f;
	AEVec2Set(&scale, ASTEROID_MAX_SCALE_X * 1.3,
		ASTEROID_MAX_SCALE_X * 1.3);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	// create the static wall
	AEVec2Set(&scale, WALL_SCALE_X, WALL_SCALE_Y);
	AEVec2 position;
	AEVec2Set(&position, 300.0f, 150.0f);
	spWall = gameObjInstCreate(TYPE_WALL, &scale, &position, nullptr, 0.0f);
	AE_ASSERT(spWall);


	// reset the score and the number of ships
	sScore      = 0;
	sShipLives  = SHIP_INITIAL_NUM;
	over = false;
}

/******************************************************************************/
/*!
	"Update" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsUpdate(void)
{

	// =========================================================
	// update according to input
	// =========================================================

	// This input handling moves the ship without any velocity nor acceleration
	// It should be changed when implementing the Asteroids project
	//
	// Updating the velocity and position according to acceleration is 
	// done by using the following:
	// Pos1 = 1/2 * a*t*t + v0*t + Pos0
	//
	// In our case we need to divide the previous equation into two parts in order 
	// to have control over the velocity and that is done by:
	//
	// v1 = a*t + v0		//This is done when the UP or DOWN key is pressed 
	// Pos1 = v1*t + Pos0
	
	if (AEInputCheckCurr(AEVK_UP) && over == false)
	{
		float acceleration = (float)SHIP_ACCEL_FORWARD * g_dt;

		AEVec2 added;
		AEVec2 acceleration_vec;
		AEVec2Set(&added, cosf(spShip->dirCurr), sinf(spShip->dirCurr));// calculating direction
		AEVec2Scale(&acceleration_vec, &added, acceleration);// set acceleration acceleration * direction
		AEVec2Add(&spShip->velCurr, &spShip->velCurr, &acceleration_vec);
		AEVec2Scale(&spShip->velCurr, &spShip->velCurr, 0.99f);

		// Find the velocity according to the acceleration
		// Limit your speed over here

	}

	if (AEInputCheckCurr(AEVK_DOWN) && over == false)
	{
		float acceleration = (float)SHIP_ACCEL_BACKWARD * g_dt;

		AEVec2 added;
		AEVec2 acceleration_vec;
		AEVec2Set(&added, -cosf(spShip->dirCurr), -sinf(spShip->dirCurr));// calculating direction
		AEVec2Scale(&acceleration_vec, &added, acceleration);// set acceleration acceleration * direction
		AEVec2Add(&spShip->velCurr, &spShip->velCurr, &acceleration_vec);//YOU MAY NEED TO CHANGE/REPLACE THIS LINE
		AEVec2Scale(&spShip->velCurr, &spShip->velCurr, 0.99f);
	}

	if (AEInputCheckCurr(AEVK_LEFT) && over == false)
	{
		spShip->dirCurr += SHIP_ROT_SPEED * (float)(AEFrameRateControllerGetFrameTime ());
		spShip->dirCurr =  AEWrap(spShip->dirCurr, -PI, PI);
	}

	if (AEInputCheckCurr(AEVK_RIGHT) && over == false)
	{
		spShip->dirCurr -= SHIP_ROT_SPEED * (float)(AEFrameRateControllerGetFrameTime ());
		spShip->dirCurr =  AEWrap(spShip->dirCurr, -PI, PI);
	}
	//AEVec2Scale(&spShip->velCurr, &spShip->velCurr, 0.99f);//fraction

	// Shoot a bullet if space is triggered (Create a new object instance)
	if (AEInputCheckTriggered(AEVK_SPACE) && over == false)
	{
		// Get the bullet's direction according to the ship's direction
		// Set the velocity
		// Create an instance, based on BULLET_SCALE_X and BULLET_SCALE_Y
		AEVec2 scale;
		AEVec2 bullet_vel;
		AEVec2Set(&bullet_vel, cosf(spShip->dirCurr), sinf(spShip->dirCurr));
		AEVec2Scale(&bullet_vel, &bullet_vel, BULLET_SPEED);
		AEVec2Set(&scale, BULLET_SCALE_X, BULLET_SCALE_Y);// scale
		gameObjInstCreate(TYPE_BULLET, &scale, &spShip->posCurr, &bullet_vel, spShip->dirCurr);
	}






	// ======================================================================
	// Save previous positions
	//  -- For all instances
	// [DO NOT UPDATE THIS PARAGRAPH'S CODE]
	// ======================================================================
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		pInst->posPrev.x = pInst->posCurr.x;
		pInst->posPrev.y = pInst->posCurr.y;
	}






	// ======================================================================
	// update physics of all active game object instances
	//  -- Calculate the AABB bounding rectangle of the active instance, using the starting position:
	//		boundingRect_min = -(BOUNDING_RECT_SIZE/2.0f) * instance->scale + instance->posPrev
	//		boundingRect_max = +(BOUNDING_RECT_SIZE/2.0f) * instance->scale + instance->posPrev
	//
	//	-- New position of the active instance is updated here with the velocity calculated earlier
	// ======================================================================

	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
	{
		GameObjInst* instance = sGameObjInstList + i;
		if ((instance->flag & FLAG_ACTIVE) == 0)
			continue;
		AEVec2 tmp;
		AEVec2Scale(&tmp, &instance->scale, BOUNDING_RECT_SIZE / 2.0f);
		AEVec2Sub(&instance->boundingBox.min, &instance->posPrev, &tmp);
		AEVec2Add(&instance->boundingBox.max, &instance->posPrev, &tmp);

		instance->posCurr.x += instance->velCurr.x * g_dt;// pos1 = po0 + v1 * dt
		instance->posCurr.y += instance->velCurr.y * g_dt;
	}




	// ======================================================================
	// check for dynamic-static collisions (one case only: Ship vs Wall)
	// [DO NOT UPDATE THIS PARAGRAPH'S CODE]
	// ======================================================================
	Helper_Wall_Collision();





	// ======================================================================
	// check for dynamic-dynamic collisions
	// ======================================================================

	/*
	for each object instance: oi1
		if oi1 is not active
			skip

		if oi1 is an asteroid
			for each object instance oi2
				if(oi2 is not active or oi2 is an asteroid)
					skip

				if(oi2 is the ship)
					Check for collision between ship and asteroids (Rectangle - Rectangle)
					Update game behavior accordingly
					Update "Object instances array"
				else
				if(oi2 is a bullet)
					Check for collision between bullet and asteroids (Rectangle - Rectangle)
					Update game behavior accordingly
					Update "Object instances array"
	*/

	if (over == false) {
		for (int i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i) {
			GameObjInst* oi1 = sGameObjInstList + i;

			if ((oi1->flag & FLAG_ACTIVE) == 0)
				continue;
			if (oi1->pObject->type == TYPE_ASTEROID) {// oi1 = asteroid
				for (int j = 0; j < GAME_OBJ_INST_NUM_MAX; ++j) {
					// Skip checking against itself
					/*if (i == j)
						continue;*/
					int random_value_x = 0;
					int random_value_y = 0;
					int random_x = 0;
					int random_y = 0;
					int random_vel_x = 0;
					int random_vel_y = 0;

					if (rand() % 2 == 0) { // Generate a random value within the range -500 to -400
						random_value_x = -400 - rand() % 101;
						random_x = -400 - rand() % 101;
					}
					else { // Generate a random value within the range 400 to 500
						random_value_x = 400 + rand() % 101;
						random_x = 400 + rand() % 101;
					}
					if (rand() % 2 == 0) { // Generate a random value within the range -400 to -300
						random_value_y = -300 - rand() % 101;
						random_y = -300 - rand() % 101;
					}
					else { // Generate a random value within the range 300 to 400
						random_value_y = 300 + rand() % 101;
						random_y = 300 + rand() % 101;
					}
					GameObjInst* oi2 = sGameObjInstList + j;
					if ((oi2->flag & FLAG_ACTIVE) == 0 || oi2->pObject->type == TYPE_ASTEROID)
						continue;//oi2 is not active or oi2 is an asteroid

					if (oi2->pObject->type == TYPE_SHIP) { // collision between ship and asteroid
						float tFirst;
						if (CollisionIntersection_RectRect(oi2->boundingBox, oi2->velCurr, oi1->boundingBox, oi1->velCurr, tFirst)) { // check collision between ship and asteroid
							gameObjInstDestroy(oi1);// destroy asteroid
							onValueChange = true;
							sShipLives--;
							//reset ship 
							AEVec2Set(&oi2->posCurr, 0.0f, 0.0f);
							AEVec2Set(&oi2->velCurr, 0.0f, 0.0f);

							oi2->dirCurr = 0.0f;
							//add 1 asteroid
							float randomValue = 0.5f + (float)rand() / (float)(RAND_MAX / 1.0f);
							if (rand() % 2 == 0) {
								random_vel_x = -30 - rand() % 101;
							}
							else {
								random_vel_x = 30 + rand() % 101;
							}
							if (rand() % 2 == 0) {
								random_vel_y = -30 - rand() % 101;
							}
							else {
								random_vel_y = 30 + rand() % 101;
							}
							AEVec2 pos{}, vel{}, scale{};
							pos.x = (float)random_x;		pos.y = (float)random_y;
							vel.x = (float)random_vel_x;		vel.y = (float)random_vel_y;
							AEVec2Set(&scale, ASTEROID_MAX_SCALE_X * randomValue,
								ASTEROID_MAX_SCALE_Y * randomValue);
							gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

						}
					}
					else {
						if (oi2->pObject->type == TYPE_BULLET) {// collision between bullet and asteroid
							float tFirst;
							if (CollisionIntersection_RectRect(oi1->boundingBox, oi1->velCurr, oi2->boundingBox, oi2->velCurr, tFirst)) {
								//update game behaviour
								sScore += 100;
								onValueChange = true;
								gameObjInstDestroy(oi1);// destroy the asteroid
								gameObjInstDestroy(oi2);// destroy the bullet

								// randomly add 1 or  2 asteroid
								int add_asteroid_num = rand() % 2;
								AEVec2 pos{}, vel{}, scale{};
								for (int k = 0; k < add_asteroid_num + 1; ++k) {
									float randomValue = 0.5f + (float)rand() / (float)(RAND_MAX / 1.0f);
									if (rand() % 2 == 0) {
										random_vel_x = -30 - rand() % 101;// rand from -130 to -100
									}
									else {
										random_vel_x = 30 + rand() % 101;// rand from 130 to 100
									}
									if (rand() % 2 == 0) {
										random_vel_y = -30 - rand() % 101;//rand from -130 to 100
									}
									else {
										random_vel_y = 30 + rand() % 101;// rand from -130 ro 100
									}
									pos.x = (float)random_value_x;		pos.y = (float)random_value_y;
									vel.x = (float)random_vel_x;		vel.y = (float)random_vel_y;
									AEVec2Set(&scale, ASTEROID_MAX_SCALE_X * randomValue,
										ASTEROID_MAX_SCALE_Y * randomValue);
									gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);
								}
							}
						}
					}
				}
			}
		}
	}
	

	// ===================================================================
	// update active game object instances
	// Example:
	//		-- Wrap specific object instances around the world (Needed for the assignment)
	//		-- Removing the bullets as they go out of bounds (Needed for the assignment)
	//		-- If you have a homing missile for example, compute its new orientation 
	//			(Homing missiles are not required for the Asteroids project)
	//		-- Update a particle effect (Not required for the Asteroids project)
	// ===================================================================
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst * pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;
		
		// check if the object is a ship
		if (pInst->pObject->type == TYPE_SHIP)
		{
			// Wrap the ship from one end of the screen to the other
			pInst->posCurr.x = AEWrap(pInst->posCurr.x, AEGfxGetWinMinX() - SHIP_SCALE_X, 
														AEGfxGetWinMaxX() + SHIP_SCALE_X);
			pInst->posCurr.y = AEWrap(pInst->posCurr.y, AEGfxGetWinMinY() - SHIP_SCALE_Y,
														AEGfxGetWinMaxY() + SHIP_SCALE_Y);
		}

		// Wrap asteroids here
		if (pInst->pObject->type == TYPE_ASTEROID)
		{
			// Wrap the asteroid from one end of the screen to the other
			pInst->posCurr.x = AEWrap(pInst->posCurr.x, AEGfxGetWinMinX() - pInst->scale.x,
				AEGfxGetWinMaxX() + pInst->scale.y);
			pInst->posCurr.y = AEWrap(pInst->posCurr.y, AEGfxGetWinMinY() - pInst->scale.x,
				AEGfxGetWinMaxY() + pInst->scale.y);
		}

		// Remove bullets that go out of bounds
		if (pInst->pObject->type == TYPE_BULLET) {
			if (pInst->posCurr.x < AEGfxGetWinMinX() - BULLET_SCALE_X ||
				pInst->posCurr.x > AEGfxGetWinMaxX() + BULLET_SCALE_X ||
				pInst->posCurr.y < AEGfxGetWinMinY() - BULLET_SCALE_Y ||
				pInst->posCurr.y > AEGfxGetWinMaxY() + BULLET_SCALE_Y) {
				gameObjInstDestroy(pInst);
			}
		}
	}



	

	// =====================================================================
	// calculate the matrix for all objects
	// =====================================================================

	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst * pInst = sGameObjInstList + i;
		AEMtx33		 trans{}, rot{}, scale{};

		UNREFERENCED_PARAMETER(trans);
		UNREFERENCED_PARAMETER(rot);
		UNREFERENCED_PARAMETER(scale);

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		// Compute the scaling matrix
		// Compute the rotation matrix 
		// Compute the translation matrix
		// Concatenate the 3 matrix in the correct order in the object instance's "transform" matrix
		AEMtx33Scale(&scale, pInst->scale.x, pInst->scale.y);
		AEMtx33Rot(&rot, pInst->dirCurr);
		AEMtx33Trans(&trans, pInst->posCurr.x, pInst->posCurr.y);

		pInst->transform = { 0.f };
		AEMtx33Concat(&pInst->transform, &rot, &scale);
		AEMtx33Concat(&pInst->transform, &trans, &pInst->transform);

	}
}

/******************************************************************************/
/*!
	
*/
/******************************************************************************/
void GameStateAsteroidsDraw(void)
{
	// Draw background
	AEGfxSetBackgroundColor(0.0f, 0.0f, 0.0f);
	/*AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
	AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	AEGfxSetTransparency(0.5f);
	AEGfxTextureSet(pTex, 0, 0);
	AEMtx33 backgroundScale;
	AEMtx33Scale(&backgroundScale, (f32)AEGfxGetWindowWidth(), (f32)AEGfxGetWindowHeight());
	AEGfxSetTransform(backgroundScale.m);
	AEGfxMeshDraw(pMesh, AE_GFX_MDM_TRIANGLES);*/

	char strBuffer[1024];

	// draw all object instances in the list
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst * pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;
		
		// Set the current object instance's transform matrix using "AEGfxSetTransform"
		// Draw the shape used by the current object instance using "AEGfxMeshDraw"
		if (pInst->pObject->type == TYPE_ASTEROID) {// draw asteroid
			AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
			AEGfxTextureSet(pTex1, 0, 0);
			AEGfxSetBlendMode(AE_GFX_BM_BLEND);
			AEGfxSetTransparency(1.0f);
			AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
			AEGfxSetTransform(pInst->transform.m);
			AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);
		}
		else if(pInst->pObject->type == TYPE_SHIP){// draw ship
			AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
			AEGfxTextureSet(pTexship, 0, 0);
			AEGfxSetBlendMode(AE_GFX_BM_BLEND);
			AEGfxSetTransparency(1.0f);
			AEGfxSetColorToMultiply(1.0f, 1.0f, 1.0f, 1.0f);
			AEGfxSetTransform(pInst->transform.m);
			AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);
		}
		else {
			AEGfxSetRenderMode(AE_GFX_RM_COLOR);
			AEGfxTextureSet(nullptr, 0, 0);
			AEGfxSetBlendMode(AE_GFX_BM_BLEND);
			AEGfxSetTransparency(1.0f);
			AEGfxSetTransform(pInst->transform.m);
			AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);
		}
		

	}

	//You can replace this condition/variable by your own data.
	//The idea is to display any of these variables/strings whenever a change in their value happens
	if(onValueChange)
	{
		sprintf_s(strBuffer, "Score: %d", sScore);
		//AEGfxPrint(10, 10, (u32)-1, strBuffer);
		printf("%s \n", strBuffer);

		sprintf_s(strBuffer, "Ship Left: %d", sShipLives >= 0 ? sShipLives : 0);
		//AEGfxPrint(600, 10, (u32)-1, strBuffer);
		printf("%s \n", strBuffer);

		// display the game over message
		
		if (sShipLives < 0 && over == false)
		{
			//AEGfxPrint(280, 260, 0xFFFFFFFF, "       GAME OVER       ");
			printf("       GAME OVER       \n");
			over = true;
		}
		if (sScore >= 5000 && over == false) {
			printf("        You Rock       \n");
			over = true;
		}
		onValueChange = false;
	}
	f32 w, h;
	std::string shiplive = "  Ship lives : " + std::to_string(sShipLives + 1);
	const char* pText = shiplive.c_str();
	AEGfxGetPrintSize(pFont, pText, 0.5f, &w, &h);
	AEGfxPrint(pFont, pText, -1.f, 1.f - h, 0.3f, 1, 1, 1, 1);

	std::string score = "  Score       : " + std::to_string(sScore);
	const char* pText2 = score.c_str();
	AEGfxGetPrintSize(pFont, pText2, 1.0f, &w, &h);
	AEGfxPrint(pFont, pText2, -1.f, 1.f - h, 0.3f, 1, 1, 1, 1);
	if (over) { // over state
		if (sShipLives < 0) {
			AEGfxGetPrintSize(pFont, "GAME OVER", 1.f, &w, &h);
			AEGfxPrint(pFont, "GAME OVER", -w / 2, -h / 2, 1, 1, 0.831f, 0.22f, 1);// print in middle
		}
		else {
			AEGfxGetPrintSize(pFont, "YOU WIN", 1.f, &w, &h);
			AEGfxPrint(pFont, "YOU WIN", -w / 2, -h / 2, 1, 1, 0.831f, 0.22f, 1);
		}
		// reset ship pos, vel and dir
		AEVec2Set(&spShip->posCurr, 0.0f, 0.0f);
		AEVec2Set(&spShip->velCurr, 0.0f, 0.0f);
		spShip->dirCurr = 0.0f;
	}
}


/******************************************************************************/
/*!
	
*/
/******************************************************************************/
void GameStateAsteroidsFree(void)
{
	// kill all object instances in the array using "gameObjInstDestroy"
	for (unsigned long int i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i) {
		GameObjInst* pInt = sGameObjInstList + i;
		gameObjInstDestroy(pInt);
	}
}

/******************************************************************************/
/*!
	
*/
/******************************************************************************/
void GameStateAsteroidsUnload(void)
{
	// free all mesh data (shapes) of each object using "AEGfxTriFree"
	for (unsigned long int i = 0; i < sGameObjNum; ++i) {
		GameObj* pObj = sGameObjList + i;
		AEGfxMeshFree(pObj->pMesh);
	}
	// free background mesh
	AEGfxMeshFree(pMesh);
	AEGfxDestroyFont(pFont);
}

/******************************************************************************/
/*!
	
*/
/******************************************************************************/
GameObjInst * gameObjInstCreate(unsigned long type, 
							   AEVec2 * scale,
							   AEVec2 * pPos, 
							   AEVec2 * pVel, 
							   float dir)
{
	AEVec2 zero;
	AEVec2Zero(&zero);

	AE_ASSERT_PARM(type < sGameObjNum);
	
	// loop through the object instance list to find a non-used object instance
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst * pInst = sGameObjInstList + i;

		// check if current instance is not used
		if (pInst->flag == 0)
		{
			// it is not used => use it to create the new instance
			pInst->pObject	= sGameObjList + type;
			pInst->flag		= FLAG_ACTIVE;
			pInst->scale	= *scale;
			pInst->posCurr	= pPos ? *pPos : zero;
			pInst->velCurr	= pVel ? *pVel : zero;
			pInst->dirCurr	= dir;
			
			// return the newly created instance
			return pInst;
		}
	}

	// cannot find empty slot => return 0
	return 0;
}

/******************************************************************************/
/*!
	
*/
/******************************************************************************/
void gameObjInstDestroy(GameObjInst * pInst)
{
	// if instance is destroyed before, just return
	if (pInst->flag == 0)
		return;

	// zero out the flag
	pInst->flag = 0;
}



/******************************************************************************/
/*!
    check for collision between Ship and Wall and apply physics response on the Ship
		-- Apply collision response only on the "Ship" as we consider the "Wall" object is always stationary
		-- We'll check collision only when the ship is moving towards the wall!
	[DO NOT UPDATE THIS PARAGRAPH'S CODE]
*/
/******************************************************************************/
void Helper_Wall_Collision()
{
	//calculate the vectors between the previous position of the ship and the boundary of wall
	AEVec2 vec1{};
	vec1.x = spShip->posPrev.x - spWall->boundingBox.min.x;
	vec1.y = spShip->posPrev.y - spWall->boundingBox.min.y;
	AEVec2 vec2{};
	vec2.x = 0.0f;
	vec2.y = -1.0f;
	AEVec2 vec3{};
	vec3.x = spShip->posPrev.x - spWall->boundingBox.max.x;
	vec3.y = spShip->posPrev.y - spWall->boundingBox.max.y;
	AEVec2 vec4{};
	vec4.x = 1.0f;
	vec4.y = 0.0f;
	AEVec2 vec5{};
	vec5.x = spShip->posPrev.x - spWall->boundingBox.max.x;
	vec5.y = spShip->posPrev.y - spWall->boundingBox.max.y;
	AEVec2 vec6{};
	vec6.x = 0.0f;
	vec6.y = 1.0f;
	AEVec2 vec7{};
	vec7.x = spShip->posPrev.x - spWall->boundingBox.min.x;
	vec7.y = spShip->posPrev.y - spWall->boundingBox.min.y;
	AEVec2 vec8{};
	vec8.x = -1.0f;
	vec8.y = 0.0f;
	if (
		(AEVec2DotProduct(&vec1, &vec2) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec2) <= 0.0f) ||
		(AEVec2DotProduct(&vec3, &vec4) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec4) <= 0.0f) ||
		(AEVec2DotProduct(&vec5, &vec6) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec6) <= 0.0f) ||
		(AEVec2DotProduct(&vec7, &vec8) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec8) <= 0.0f)
		)
	{
		float firstTimeOfCollision = 0.0f;
		if (CollisionIntersection_RectRect(spShip->boundingBox,
			spShip->velCurr,
			spWall->boundingBox,
			spWall->velCurr,
			firstTimeOfCollision))
		{
			//re-calculating the new position based on the collision's intersection time
			spShip->posCurr.x = spShip->velCurr.x * (float)firstTimeOfCollision + spShip->posPrev.x;
			spShip->posCurr.y = spShip->velCurr.y * (float)firstTimeOfCollision + spShip->posPrev.y;

			//reset ship velocity
			spShip->velCurr.x = 0.0f;
			spShip->velCurr.y = 0.0f;
		}
	}
}