#define _USE_MATH_DEFINES
#include <GL/glew.h>
#include <GL/glut.h>
#include <math.h>       /* for cos(), sin(), and sqrt() */
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include "kiss-skeleton.h"
#include "zgfx.h"

// global vars
static const char *skelfile, *meshfile;
SkeletonPose *editPose; // the "pose mode" current pose

void mouse(int button, int state, int x, int y)
{
    // Left button possible starts an edit
    if (button == GLUT_LEFT_BUTTON)
    {
        // Handles timeline actions
        {
            // Select a frame
            if (state == GLUT_UP) return;

        }
    }

    glutPostRedisplay();
}

void keyboard(GLubyte key, GLint x, GLint y)
{
    // Quit on ESC
    if (key == 27)
        exit(0);
    if (key == 'b')
    {
        skeleton->setBindPose();
        freeSkeletonPose(bindPose);
        bindPose = currentPose();
        freeSkeletonPose(editPose);
        editPose = currentPose();
        autoSkinMesh();
        free(verts);
        verts = createSkinnedVertArray(rmesh, &nverts);
    }
    // Tab
    if (key == 9 && meshMode != NO_MESH_MODE)
    {
        if (meshMode == POSING_MODE)
        {
            // Save the edit pose
            freeSkeletonPose(editPose);
            editPose = currentPose();

            meshMode = SKINNING_MODE;
            skeleton->setPose(bindPose);
        }
        else if (meshMode == SKINNING_MODE)
        {
            // restore the edit pose
            skeleton->setPose(editPose);
            meshMode = POSING_MODE;
        }
    }
    // reset to bind pose
    if (key == '\\')
    {
        skeleton->setPose(bindPose);
    }
    if (key == 'x' && rmesh)
    {
        writeRawMesh(rmesh, "mesh.out.obj");
        writeSkeleton("skeleton.out.bones");
    }
    // Keyframing: 'k' to create keyframe, 'l' to delete it.
    if (key == 'k' && meshMode == POSING_MODE)
    {
        keyframes[currentFrame] = currentPose();
    }
    if (key == 'K' && meshMode == POSING_MODE)
    {
		// TODO delete keyframe (using freeSkeletonPose)
        keyframes.erase(currentFrame); 
    }
    if (key == ' ' && meshMode == POSING_MODE)
    {
        play = !play;
        std::cout << "playing: " << play << std::endl;
        if (play)
            glutTimerFunc(1000 / FPS, playTimelineFunc, 0);
    }
    if (key == 'l')
        localMode = !localMode;
    // Update display...
    glutPostRedisplay();
}

void playTimelineFunc(int val)
{
    if (currentFrame == endFrame)
        setFrame(startFrame);
    else
        setFrame(currentFrame + 1);
    if (play)
        glutTimerFunc(1000 / FPS, playTimelineFunc, 0);
}

