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

// constants
static int windowWidth = 800, windowHeight = 1000;
static int timelineHeight = 200;
static const int FPS = 24;

// global vars
static rawmesh *rmesh;
static shader  *shader;
static const char *skelfile, *meshfile;
static vert_p4t2n3j8 *verts;
static size_t nverts;
SkeletonPose *editPose; // the "pose mode" current pose

// Timeline vars

// translation mode variables
static glm::vec3 startingPos; // the parent space starting pos of selectedJoint
static glm::vec3 translationVec(0.f);
// rotation mode variables
static glm::vec3 rotationVec;
static glm::quat startingRot;
// scaling mode variables
static float startingScale = 1.f;

// Functions
static glm::vec2 clickToScreenPos(int x, int y);
static glm::vec3 applyMatrix(const glm::mat4 &mat, const glm::vec3 &vec, bool homogenous = true);
// p1 and p2 determine the line, pt is the point to get distance for
// if the point is not between the line segment, then HUGE_VAL is returned
static float pointLineDist(const glm::vec2 &p1, const glm::vec2 &p2, const glm::vec2 &pt);

static void autoSkinMesh();
static SkeletonPose* currentPose();
static void writeSkeleton(const char *filename); // writes the current bind position as skeleton

static void playTimelineFunc(int val);
static void setPoseFromFrame(int frame);
static void setFrame(int frame);

void redraw(void)
{
    // Render main window
    glViewport(0, timelineHeight, windowWidth, windowHeight - timelineHeight);

    // Render timeline
    glViewport(0, 0, windowWidth, timelineHeight);
    renderTimeline();

    glutSwapBuffers();
}

void mouse(int button, int state, int x, int y)
{
    // Left button possible starts an edit
    if (button == GLUT_LEFT_BUTTON)
    {
        // Handles timeline actions
        {
            // Select a frame
            if (state == GLUT_UP) return;

            if (y >= windowHeight - 0.75 * timelineHeight && 
                    y <= windowHeight - 0.25 * timelineHeight)
            {
                setFrame(startFrame + (x * (endFrame - startFrame + 1) + windowWidth * 0.5f) / windowWidth - 1);
                if (currentFrame > endFrame) currentFrame = endFrame;
                std::cout << "Selected frame " << currentFrame << std::endl;
            }
        }
    }

    glutPostRedisplay();
}

void motion(int x, int y)

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

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("usage: %s: BONESFILE [OBJFILE]\n", argv[0]);
        exit(1);
    }
    skelfile = argv[1];
    meshfile = NULL;
    if (argc >= 3)
        meshfile = argv[2];

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH | GLUT_MULTISAMPLE);
	glutInitWindowSize(windowWidth, windowHeight);

    glutCreateWindow("kiss_particle demo");
    glutDisplayFunc(redraw);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);

    glEnable(GL_DEPTH_TEST);

    // --------------------
    // START MY SETUP
    // --------------------

    editPose = currentPose();

    // --------------------
    // END MY SETUP
    // --------------------

    glutMainLoop();
    return 0;             /* ANSI C requires main to return int. */
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

void setPoseFromFrame(int frame)
{
    if (keyframes.count(frame) > 0)
    {
        skeleton->setPose(keyframes[frame]);
    } 
    else if (keyframes.size() > 1)
    {
        int lastKeyframe = currentFrame;
        int nextKeyframe = currentFrame;
        while (keyframes.count(lastKeyframe) == 0 && lastKeyframe > startFrame)
            lastKeyframe--;
        while (keyframes.count(nextKeyframe) == 0 && nextKeyframe < endFrame)
            nextKeyframe++;
        if (keyframes.count(lastKeyframe) == 0)
            lastKeyframe = nextKeyframe;
        if (keyframes.count(nextKeyframe) == 0)
            nextKeyframe = lastKeyframe;
        
        // The normalized position of the current frame between the keyframes
        float anim = 0.f;
        if (nextKeyframe - lastKeyframe > 0)
            anim = float(currentFrame - lastKeyframe) / (nextKeyframe - lastKeyframe);

		assert(keyframes.count(lastKeyframe) && keyframes.count(nextKeyframe));
		const SkeletonPose *last = keyframes[lastKeyframe];
		const SkeletonPose *next = keyframes[nextKeyframe];
		assert(last->poses.size() == next->poses.size());

		SkeletonPose* pose = new SkeletonPose;
		pose->poses.resize(last->poses.size());
        for (int i = 0; i < pose->poses.size(); i++)
        {
			JointPose *jp = pose->poses[i] = new JointPose;
            glm::vec3 startPos = last->poses[i]->pos;
            glm::vec3 endPos = next->poses[i]->pos;
            jp->pos = startPos + (endPos - startPos) * anim;

            glm::quat startRot = axisAngleToQuat(last->poses[i]->rot);
            glm::quat endRot = axisAngleToQuat(next->poses[i]->rot);
            jp->rot = quatToAxisAngle(glm::normalize((1.f - anim) * startRot + anim * endRot));

            float startScale = last->poses[i]->scale;
            float endScale = next->poses[i]->scale;
            jp->scale = startScale + (endScale - startScale) * anim;
        }
        
        skeleton->setPose(pose);
		freeSkeletonPose(pose);
    }
}

void setFrame(int frame)
{
    currentFrame = frame;
    setPoseFromFrame(frame);
    glutPostRedisplay();
}

