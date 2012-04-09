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
#include "arcball.h"
#include "kiss-skeleton.h"
#include "zgfx.h"

// constants
static int windowWidth = 800, windowHeight = 1000;
static int timelineHeight = 200;
static const float arcballRadius = 10.f;
static const float zoomSpeed = 2.f;
static const float selectThresh = 0.02f;
static const float axisLength = 0.10f; // ndc
static const float circleRadius = 0.12f; //ndc
static const float scaleCircleRadius = 0.15f; //ndc
static const int TRANSLATION_MODE = 1, ROTATION_MODE = 2, SCALE_MODE = 3;
static const int NO_MESH_MODE = 0, SKINNING_MODE = 1, POSING_MODE = 2;
static const int FPS = 24;

static const glm::vec3 selColor(0.8f, 0.4f, 0.2f);

// global vars
static Skeleton *skeleton;
static Arcball *arcball;
static rawmesh *rmesh;
static shader  *shader;
static const char *skelfile, *meshfile;
static vert_p4t2n3j8 *verts;
static size_t nverts;
SkeletonPose *bindPose;
SkeletonPose *editPose; // the "pose mode" current pose
static std::vector<glm::vec3> jointNDC;
static glm::vec3 axisDir[3]; // x,y,z axis directions
static glm::vec3 axisNDC[3]; // x,y,z axis marker endpoints
static glm::vec3 circleNDC;  // circle ndc coordinate

// UI vars
static const Joint *selectedJoint = NULL;
static glm::vec2 dragStart;
static float zoomStart; // starting zoom level of a zoom drag
static bool rotating = false;
static bool dragging = false;
static bool zooming  = false;
static bool translating = false;
static int meshMode = NO_MESH_MODE;
static int editMode = TRANSLATION_MODE;
static bool localMode = false;

// Timeline vars
static int startFrame = 1;
static int endFrame = 60;
static int currentFrame = 1;
static std::map<int, SkeletonPose*> keyframes;
static bool play;

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

static void setTranslationVec(const glm::vec2 &clickPos);
static void setRotationVec(const glm::vec2 &clickPos);
static void setScaleVec(const glm::vec2 &clickPos);

static void setJointPosition(const Joint* joint, const glm::vec2 &dragPos);
static void setJointRotation(const Joint* joint, const glm::vec2 &dragPos);
static void setJointScale(const Joint* joint, const glm::vec2 &dragPos);

static void renderCube();
static void renderJoint(const glm::mat4 &viewTransform, const Joint* joint, const std::vector<Joint*> joints);
static void renderAxes(const glm::mat4 &viewTransform, const glm::vec3 &worldCoord);
static void renderLine(const glm::vec4 &transform, const glm::vec3 &p0, const glm::vec3 &p1);
static void renderScaleCircle(const glm::mat4 &viewTransform, const glm::vec3 &worldCoord);
static void renderCircle(const glm::mat4 &worldTransform);
static void renderRotationSphere(const glm::mat4 &worldTransform, const glm::vec3 &worldCoord);
static void renderPoints(const glm::mat4 &transform, vert *verts, size_t nverts);
static void renderSelectedPoints(const glm::mat4 &transform, rawmesh *mesh, int joint,
        const glm::vec4 &color);
static void renderMesh(const glm::mat4 &transform, const vert_p4t2n3j8 *verts, size_t nverts);
static void renderSkinnedMesh(const glm::mat4 &transform, const vert_p4t2n3j8 *verts,
        size_t nverts, const glm::vec4 &color);
static void renderEditGrid();
static void renderTimeline();

static glm::mat4 getViewMatrix();
static glm::mat4 getProjectionMatrix();
std::ostream& operator<< (std::ostream& os, const glm::vec2 &v);
std::ostream& operator<< (std::ostream& os, const glm::vec3 &v);
std::ostream& operator<< (std::ostream& os, const glm::vec4 &v);
std::ostream& operator<< (std::ostream& os, const glm::quat &v);

// quaternion functions
static glm::quat axisAngleToQuat(const glm::vec4 &axisAngle); // input vec: (axis, angle (deg))
static glm::vec4 quatToAxisAngle(const glm::quat &q); // output vec (axis, angle (deg))

void redraw(void)
{
    // Now render
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glm::mat4 projMatrix = arcball->getProjectionMatrix();
    glLoadMatrixf(glm::value_ptr(projMatrix));

    glMatrixMode(GL_MODELVIEW);
    glm::mat4 viewMatrix = arcball->getViewMatrix();
    glLoadMatrixf(glm::value_ptr(viewMatrix));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render main window
    glViewport(0, timelineHeight, windowWidth, windowHeight - timelineHeight);

    renderEditGrid();

    // Render the joints
    const std::vector<Joint*> joints = skeleton->getJoints();
    for (size_t i = 0; i < joints.size(); i++)
        renderJoint(viewMatrix, joints[i], joints);

    // And the mesh
    if (meshMode == SKINNING_MODE)
    {
        glColor3f(1.f, 1.f, 1.f);
        renderPoints(viewMatrix, rmesh->verts, rmesh->nverts);

        glColor4fv(glm::value_ptr(glm::vec4(selColor, 0.5f)));
        renderMesh(viewMatrix, verts, nverts);

        if (selectedJoint)
        {
            renderSelectedPoints(viewMatrix, rmesh, selectedJoint->index,
                    glm::vec4(0,1,0,1));
        }
    }
    else if (meshMode == POSING_MODE)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        renderSkinnedMesh(viewMatrix, verts, nverts, glm::vec4(0.7f));
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Render timeline
    glViewport(0, 0, windowWidth, timelineHeight);
    renderTimeline();

    glutSwapBuffers();
}

void reshape(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    windowWidth = width;
    windowHeight = height;

    glViewport(0, 0, windowWidth, windowHeight);

    arcball->setAspect((float) windowWidth / (windowHeight - timelineHeight));
}

void mouse(int button, int state, int x, int y)
{
    // Left button possible starts an edit
    if (button == GLUT_LEFT_BUTTON)
    {
        // Handles main window actions
        if(y <= windowHeight - timelineHeight)
        {
            // If button up, nothing to do...
            if (state == GLUT_UP)
            {
                dragging = false;
                return;
            }
            // If no joint selected, nothing to do
            if (!selectedJoint) return;

            glm::vec2 clickPos = clickToScreenPos(x, y);
            if (clickPos == glm::vec2(HUGE_VAL))
                return;
            if (editMode == TRANSLATION_MODE)
                setTranslationVec(clickPos);
            else if (editMode == ROTATION_MODE)
                setRotationVec(clickPos);
            else if (editMode == SCALE_MODE)
                setScaleVec(clickPos);
            else
                assert(false && "unknown mode");
        }
        // Handles timeline actions
        else
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
    // Right mouse selects and deselects bones
    else if (button == GLUT_RIGHT_BUTTON)
    {
        if (state == GLUT_DOWN)
        {
            selectedJoint = NULL;
            glm::vec2 screencoord = clickToScreenPos(x, y);

            // Find closest joint
            float nearest = HUGE_VAL;
            for (size_t i = 0; i < jointNDC.size(); i++)
            {
                const glm::vec3 &ndc = jointNDC[i];
                float dist = glm::length(screencoord - glm::vec2(ndc));
                if (dist < selectThresh && ndc.z < nearest)
                {
                    selectedJoint = skeleton->getJoint(i);
                    nearest = ndc.z;
                }
            }
        }
    }
    // Middle button rotates camera
    else if (button == GLUT_MIDDLE_BUTTON)
    {
        if (state == GLUT_DOWN)
        {
            if (glutGetModifiers() & GLUT_ACTIVE_CTRL)
            {
                dragStart = clickToScreenPos(x, y);
                zoomStart = arcball->getZoom();
                zooming = true;
            }
            else if (glutGetModifiers() & GLUT_ACTIVE_SHIFT)
            {
                dragStart = clickToScreenPos(x, y);
				arcball->start(glm::vec3(dragStart, 0.f));
                translating = true;
            }
            else if (!glutGetModifiers())
            {
                glm::vec2 screenPos = clickToScreenPos(x, y);
                glm::vec3 ndc(screenPos, 0.f);
                arcball->start(ndc);
                rotating = true;
            }
        }
        else
        {
            rotating = false;
            zooming = false;
            translating = false;
        }
    }
    // Mouse wheel (buttons 3 and 4) zooms in an out (translates camera)
    else if (button == 3 || button == 4)
    {
        const float zoomSpeed = 1.1f;
        float fact = (button == 4) ? zoomSpeed : 1.f/zoomSpeed;
        arcball->setZoom(arcball->getZoom() * fact);
    }

    glutPostRedisplay();
}

void motion(int x, int y)
{
    if (dragging)
    {
        glm::vec2 screenpos = clickToScreenPos(x, y);

        if (editMode == TRANSLATION_MODE)
            setJointPosition(selectedJoint, screenpos);
        else if (editMode == ROTATION_MODE)
            setJointRotation(selectedJoint, screenpos);
        else if (editMode == SCALE_MODE)
            setJointScale(selectedJoint, screenpos);
        else
            assert(false && "Unknown edit mode");
    }
    else if (rotating)
    {
        glm::vec3 ndc(clickToScreenPos(x, y), 0.f);
        if (ndc != glm::vec3(HUGE_VAL, HUGE_VAL, 0.f))
            arcball->move(ndc);
    }
    else if (zooming)
    {
        glm::vec2 delta = clickToScreenPos(x, y) - dragStart;
        
        // only the y coordinate of the drag matters
        float fact = delta.y;

        float newZoom = zoomStart * powf(zoomSpeed, fact);
        arcball->setZoom(newZoom);
    }
    else if (translating)
    {
        glm::vec2 delta = clickToScreenPos(x, y) - dragStart;
        arcball->translate(-10.f * delta);
    }

    glutPostRedisplay();
}

void keyboard(GLubyte key, GLint x, GLint y)
{
    // Quit on ESC
    if (key == 27)
        exit(0);
    if (key == 't')
        editMode = TRANSLATION_MODE;
    if (key == 'r')
        editMode = ROTATION_MODE;
    if (key == 's')
        editMode = SCALE_MODE;
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
        freeSkeletonPose(editPose);
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

    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::cerr << "Unable to initialize GLEW: " << glewGetErrorString(err) << '\n';
        exit(1);
    }

    glEnable(GL_DEPTH_TEST);

    // --------------------
    // START MY SETUP
    // --------------------

    shader = make_program("meshskin.v.glsl", "meshskin.f.glsl");

    arcball = new Arcball(glm::vec3(0, 0, -20), 20.f, 1.f, 0.1f, 1000.f, 50.f);

    skeleton = new Skeleton();
    skeleton->readSkeleton(skelfile);
    bindPose = currentPose();
    editPose = currentPose();

    jointNDC = std::vector<glm::vec3>(skeleton->getJoints().size());

    rmesh = NULL;
    if (meshfile)
    {
		bool skinned = true;
        rmesh = loadRawMesh(meshfile, skinned);
        verts = createSkinnedVertArray(rmesh, &nverts);
        meshMode = skinned ? POSING_MODE : SKINNING_MODE;
    }

    // --------------------
    // END MY SETUP
    // --------------------

    glutMainLoop();
    return 0;             /* ANSI C requires main to return int. */
}

glm::vec2 clickToScreenPos(int x, int y)
{
    glm::vec2 screencoord((float)x / windowWidth, (float)y / (windowHeight - timelineHeight));
    screencoord -= glm::vec2(0.5f);
    screencoord *= 2.f;
    screencoord.y = -screencoord.y;

    //std::cout << "in: " << x << ' ' << y << "  coord: " << screencoord << '\n';
    return screencoord;
}

glm::vec3 applyMatrix(const glm::mat4 &mat, const glm::vec3 &vec, bool homo)
{
    glm::vec4 pt(vec, homo ? 1 : 0);
    pt = mat * pt;
    if (homo) pt /= pt.w;
    return glm::vec3(pt);
}

void autoSkinMesh()
{
    // TODO update this to use a better algorithm
    // Requires a mesh with skinning data
    assert(rmesh && rmesh->joints && rmesh->weights);

    // First get all the joint world positions for easy access
    std::vector<glm::vec3> jointPos;
    {
        const std::vector<Joint*> joints = skeleton->getJoints();
        jointPos = std::vector<glm::vec3>(joints.size());
        for (size_t i = 0; i < joints.size(); i++)
            jointPos[i] = applyMatrix(joints[i]->worldTransform, glm::vec3(0.f));
    }

    // For each vertex, find the closest 2 joints and set weights according
    // to distance
    std::vector<float> dists;
    vert  *verts   = rmesh->verts;
    int   *joints  = rmesh->joints;
    float *weights = rmesh->weights;
    for (size_t i = 0; i < rmesh->nverts; i++)
    {
        dists.assign(jointPos.size(), HUGE_VAL);
        for (size_t j = 0; j < dists.size(); j++)
        {
            glm::vec3 v = glm::make_vec3(verts[i].pos);
            dists[j] = glm::length(jointPos[j] - v);
        }

        // only take best two
        for (size_t j = 0; j < 2; j++)
        {
            // Find best joint/dist pair
            float best = HUGE_VAL;
            int bestk = -1;
            for (size_t k = 0; k < dists.size(); k++)
            {
                if (dists[k] < best)
                {
                    best = dists[k];
                    bestk = k;
                }
            }

            joints[4*i + j]  = bestk;
            weights[4*i + j] = best;
            dists[bestk] = HUGE_VAL;
        }
        // Set the rest to 0
        joints[4*i + 2] = 0;
        joints[4*i + 3] = 0;
        weights[4*i + 2] = 0.f;
        weights[4*i + 3] = 0.f;

        // normalize weights
        float sum = 0.f;
        for (size_t j = 0; j < 4; j++)
            sum += weights[4*i + j];
        for (size_t j = 0; j < 4; j++)
            if (weights[4*i + j] != 0.f)
                weights[4*i + j] = 1 - (weights[4*i + j] / sum);
    }
}

SkeletonPose* currentPose()
{
    const std::vector<Joint*> joints = skeleton->getJoints();

    SkeletonPose *pose = new SkeletonPose();
    pose->poses = std::vector<JointPose*>(joints.size());

    for (size_t i = 0; i < joints.size(); i++)
    {
        JointPose *p = new JointPose();
        const Joint *j = joints[i];
        p->pos = j->pos;
        p->rot = j->rot;
        p->scale = j->scale;
        pose->poses[i] = p;
    }

    return pose;
}

void writeAnimations(const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (!f)
    {
        fprintf(stderr, "Unable to open %s for writing.\n", filename);
        return;
    }
    std::map<int, SkeletonPose*>::iterator it;
    for (it = keyframes.begin(); it != keyframes.end(); it++)
    {
        fprintf(f, "%d\n", (*it).first);
        const std::vector<JointPose*> poses = (*it).second->poses;
        for (size_t i = 0; i < poses.size(); i++)
        {
            const Joint *j = skeleton->getJoint(i);
            const JointPose *p = poses[i];
            // format is:
            // pos(3f) rot(4f) scale(1f)
            fprintf(f, "\t %f %f %f   %f %f %f %f   %f\n",
                    p->pos[0], p->pos[1], p->pos[2],
                    p->rot[0], p->rot[1], p->rot[2], p->rot[3],
                    p->scale);
        }
 
    }
    fclose(f);
}

void writeSkeleton(const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (!f)
    {
        fprintf(stderr, "Unable to open %s for writing.\n", filename);
        return;
    }

    const std::vector<JointPose*> poses = bindPose->poses;
    for (size_t i = 0; i < poses.size(); i++)
    {
        const Joint *j = skeleton->getJoint(i);
        const JointPose *p = poses[i];
        // format is:
        // name(s) pos(3f) rot(4f) scale(1f) parent(1i)
        fprintf(f, "%s   %f %f %f   %f %f %f %f   %f %d\n",
                j->name.c_str(), p->pos[0], p->pos[1], p->pos[2],
                p->rot[0], p->rot[1], p->rot[2], p->rot[3],
                p->scale, j->parent);
    }

    fclose(f);
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

void setTranslationVec(const glm::vec2 &clickPos)
{
    // pixels to NDC
    const float axisDistThresh = 5.f / std::max(windowWidth, windowHeight);
    const float circleDistThresh = 8.f / std::max(windowWidth, windowHeight);

    glm::vec3 selJointNDC = jointNDC[selectedJoint->index];

    glm::vec3 clickNDC(clickPos, selJointNDC.z);

    // figure out what the vector is (i.e. what they clicked on, if anything)
    // First see if they clicked too far away
    float dist = glm::length(clickNDC - selJointNDC);
    if (dist > circleRadius + circleDistThresh)
        return;

    translationVec = glm::vec3(0.f);
    // now see if they clicked on any of the axis lines, within margin
    if (pointLineDist(glm::vec2(selJointNDC), glm::vec2(axisNDC[0]), glm::vec2(clickNDC)) < axisDistThresh)
        translationVec.x = 1.f;
    else if (pointLineDist(glm::vec2(selJointNDC), glm::vec2(axisNDC[1]), glm::vec2(clickNDC)) < axisDistThresh)
        translationVec.y = 1.f;
    else if (pointLineDist(glm::vec2(selJointNDC), glm::vec2(axisNDC[2]), glm::vec2(clickNDC)) < axisDistThresh)
        translationVec.z = 1.f;

    // Now the vector is either set, or they didn't click on an axis vector
    // Check for circle click by just seeing if they clicked close to the radius
    // if it's not circle click, no drag
    if (translationVec == glm::vec3(0.f) &&
            (dist < circleRadius - circleDistThresh || dist > circleRadius + circleDistThresh))
        return;

    // If they choose an axis translation vector, we need to make sure that it's in the
    // correct (parent) space
    if (localMode)
        translationVec = applyMatrix(selectedJoint->worldTransform, translationVec, false);
    glm::mat4 parentTransform = selectedJoint->parent == Skeleton::ROOT_PARENT ?
        glm::mat4(1.f) : skeleton->getJoint(selectedJoint->parent)->worldTransform;
    translationVec = applyMatrix(glm::inverse(parentTransform), translationVec, false);

    // If we got here, then it's a valid drag and translationVec is already set
    std::cout << "Translation drag.  vec: " << translationVec << '\n';
    dragging = true;
    dragStart = clickPos;
    startingPos = selectedJoint->pos;
}

void setRotationVec(const glm::vec2 &clickPos)
{
    const float axisDistThresh = 5.f / glm::max(windowWidth, windowHeight);
    const float arcDistThresh = 0.003f;
    glm::vec3 selJointNDC = jointNDC[selectedJoint->index];
    glm::vec3 clickNDC(clickPos, selJointNDC.z);
    const float r = axisLength;

    // Check if they're outside of the rendered rotation sphere
    float dist = glm::length(clickNDC - selJointNDC);
    if (dist > axisLength + axisDistThresh)
        return;

    // Now compute the location of their click on the sphere, radius small
    // Assume they've clicked on the sphere, so just map to front hemisphere
    glm::vec3 sphereLoc = glm::vec3((clickPos - glm::vec2(selJointNDC)), 0.f);
    // Now fill out z coordinate to make length appropriate
    float mag = glm::min(r, glm::length(sphereLoc));
    sphereLoc.z = sqrtf(r*r - mag*mag);
    sphereLoc = glm::normalize(sphereLoc);

    // The global rotation vector
    glm::vec3 globalVec(0.f);

    // Now figure out if any of those lie along the drawn arcs
    // Project the sphere location onto each of the axis planes (for x axis, the y,z plane)
    // Then get the length of that vector, if it's nearly 1, then they are next to the arc
    float xdist = 1.f - glm::length(sphereLoc - glm::dot(sphereLoc, axisDir[0]) * axisDir[0]);
    float ydist = 1.f - glm::length(sphereLoc - glm::dot(sphereLoc, axisDir[1]) * axisDir[1]);
    float zdist = 1.f - glm::length(sphereLoc - glm::dot(sphereLoc, axisDir[2]) * axisDir[2]);
    if (xdist < arcDistThresh)
        globalVec = glm::vec3(1, 0, 0);
    else if (ydist < arcDistThresh)
        globalVec = glm::vec3(0, 1, 0);
    else if (zdist < arcDistThresh)
        globalVec = glm::vec3(0, 0, 1);

    // If no arc selected, no drag initiated
    if (globalVec == glm::vec3(0.f))
        return;

    // Transform the rotation vector into parent space so that the transformation
    // is around the global axes
    glm::mat4 parentTransform = selectedJoint->parent == Skeleton::ROOT_PARENT ? glm::mat4(1.f) : skeleton->getJoint(selectedJoint->parent)->worldTransform;
    rotationVec = globalVec;
    if (localMode)
        rotationVec = applyMatrix(selectedJoint->worldTransform, rotationVec, false);
    rotationVec = applyMatrix(glm::inverse(parentTransform), rotationVec, false);
    // Save startign rotation as quat
    startingRot = axisAngleToQuat(selectedJoint->rot);

    dragging = true;
    dragStart = clickPos;

    std::cout << "Rotation drag.  axis: " << globalVec << "  vec: " << rotationVec << '\n';
}

void setScaleVec(const glm::vec2 &clickPos)
{
    // pixels to NDC
    const float circleDistThresh = 8.f / std::max(windowWidth, windowHeight);

    glm::vec3 selJointNDC = jointNDC[selectedJoint->index];

    glm::vec3 clickNDC(clickPos, selJointNDC.z);

    // First see if they clicked too far away
    float dist = glm::length(clickNDC - selJointNDC);
    if (dist > scaleCircleRadius + circleDistThresh || dist < scaleCircleRadius - circleDistThresh)
        return;
    dragging = true;
    dragStart = clickPos;
    startingScale = selectedJoint->scale;
    std::cout << "Scale drag.\n";
}

float pointLineDist(const glm::vec2 &p1, const glm::vec2 &p2, const glm::vec2 &pt)
{
    // from http://local.wasp.uwa.edu.au/~pbourke/geometry/pointline/
    float l = glm::length(p2 - p1);
    float u = ((pt.x - p1.x) * (p2.x - p1.x) + (pt.y - p1.y) * (p2.y - p1.y)) / (l*l);

    // if not 'on' then line segment, then inf distance
    if (u < 0.f || u > 1.f)
        return HUGE_VAL;

    glm::vec2 intersect = p1 + u * (p2 - p1);
    
    return glm::length(pt - intersect);
}

void setJointPosition(const Joint* joint, const glm::vec2 &dragPos)
{
    if (dragPos == glm::vec2(HUGE_VAL))
        return;
    glm::mat4 parentWorld = joint->parent == Skeleton::ROOT_PARENT ? glm::mat4(1.f) : skeleton->getJoint(joint->parent)->worldTransform;

    // Get the positions of the two mouse positions in parent joint space
    glm::vec3 startParentPos = applyMatrix(glm::inverse(getProjectionMatrix() * getViewMatrix() * parentWorld),
            glm::vec3(dragStart, jointNDC[selectedJoint->index].z));
    glm::vec3 mouseParentPos = applyMatrix(glm::inverse(getProjectionMatrix() * getViewMatrix() * parentWorld),
            glm::vec3(dragPos, jointNDC[selectedJoint->index].z));

    // project on translation vector if necessary
    if (translationVec != glm::vec3(0.f))
    {
        startParentPos = glm::dot(startParentPos, translationVec) * translationVec;
        mouseParentPos = glm::dot(mouseParentPos, translationVec) * translationVec;
    }

    // Delta is trivial to compute now
    glm::vec3 deltaPos = mouseParentPos - startParentPos;

    // Finally, set the joint pose
    JointPose pose;
    pose.rot = joint->rot;
    pose.scale = joint->scale;
    pose.pos = startingPos + deltaPos;
    skeleton->setPose(selectedJoint->index, &pose);
}

void setJointRotation(const Joint* joint, const glm::vec2 &dragPos)
{
    glm::vec3 ndcCoord(dragPos, jointNDC[selectedJoint->index].z);
    if (dragPos == glm::vec2(HUGE_VAL))
        return;
    glm::vec2 center(jointNDC[selectedJoint->index]);

    glm::vec2 starting = glm::normalize(dragStart - center);
    glm::vec2 current  = glm::normalize(dragPos - center);

    float angle = 180.f/M_PI * acosf(glm::dot(starting, current));
    angle *= glm::sign(glm::cross(glm::vec3(starting, 0.f), glm::vec3(current, 0.f)).z);
    glm::quat deltarot = axisAngleToQuat(glm::vec4(rotationVec, angle));

    glm::quat newrot = deltarot * startingRot;

    JointPose newpose;
    newpose.pos = joint->pos;
    newpose.scale = joint->scale;
    newpose.rot = quatToAxisAngle(newrot);

    skeleton->setPose(selectedJoint->index, &newpose);
}

void setJointScale(const Joint* joint, const glm::vec2 &dragPos)
{
    glm::vec2 boneNDC(jointNDC[selectedJoint->index]);
    float newScale = glm::length(dragPos - boneNDC) / scaleCircleRadius;
    JointPose pose;
    pose.rot = joint->rot;
    pose.pos = joint->pos;
    pose.scale = newScale;
    skeleton->setPose(selectedJoint->index, &pose);
}

static GLfloat cube_verts[] = {
    1,1,1,    -1,1,1,   -1,-1,1,  1,-1,1,        // v0-v1-v2-v3
    1,1,1,    1,-1,1,   1,-1,-1,  1,1,-1,        // v0-v3-v4-v5
    1,1,1,    1,1,-1,   -1,1,-1,  -1,1,1,        // v0-v5-v6-v1
    -1,1,1,   -1,1,-1,  -1,-1,-1, -1,-1,1,    // v1-v6-v7-v2
    -1,-1,-1, 1,-1,-1,  1,-1,1,   -1,-1,1,    // v7-v4-v3-v2
    1,-1,-1,  -1,-1,-1, -1,1,-1,  1,1,-1};   // v4-v7-v6-v5

static void renderCube()
{
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, cube_verts);

    glDrawArrays(GL_QUADS, 0, 24);

    glDisableClientState(GL_VERTEX_ARRAY);
}

void renderJoint(const glm::mat4 &transform, const Joint* joint,
        const std::vector<Joint*> joints)
{
    // Draw cube
    glLoadMatrixf(glm::value_ptr(glm::scale(transform * joint->worldTransform,
                    glm::vec3(0.08f))));
    if (joint == selectedJoint)
        glColor3fv(glm::value_ptr(selColor));
    else
        glColor3f(1, 1, 1);
    renderCube();

    // Render axis (x,y,z lines) if necessary
    if (editMode == TRANSLATION_MODE && joint == selectedJoint)
    {
        glm::vec3 axisCenter = applyMatrix(joint->worldTransform, glm::vec3(0,0,0));
        renderAxes(transform, axisCenter);
    }
    else if (editMode == ROTATION_MODE && joint == selectedJoint)
    {
        glm::vec3 pos = applyMatrix(joint->worldTransform, glm::vec3(0,0,0));
        renderRotationSphere(transform, pos);
    }
    else if (editMode == SCALE_MODE && joint == selectedJoint)
    {
        glm::vec3 pos = applyMatrix(joint->worldTransform, glm::vec3(0,0,0));
        renderScaleCircle(transform * joint->scale, pos);
    }
    // Record joint NDC coordinates
    glm::mat4 ptrans = getProjectionMatrix();
    glm::mat4 fullTransform = ptrans * transform * joint->worldTransform;
    glm::vec4 jointndc(0,0,0,1);
    jointndc = fullTransform * jointndc;
    jointndc /= jointndc.w;
    jointNDC[joint->index] = glm::vec3(jointndc);

    // No "bone" to draw for root
    if (joint->parent == 255)
        return;

    glLoadMatrixf(glm::value_ptr(transform));

    glm::mat4 parentTransform = joints[joint->parent]->worldTransform;
    glm::mat4 boneTransform = joint->worldTransform;

    glm::vec4 start(0, 0, 0, 1), end(0, 0, 0, 1);
    start = parentTransform * start;
    start /= start.w;
    end = boneTransform * end;
    end /= end.w;

    glBegin(GL_LINES);
    glColor3f(1, 0, 0);
    glVertex4fv(&start.x);

    glColor3f(0, 1, 0);
    glVertex4fv(&end.x);
    glEnd();
}

glm::mat4 getProjectionMatrix()
{
    return arcball->getProjectionMatrix();
}

glm::mat4 getViewMatrix()
{
    return arcball->getViewMatrix();
}

void renderLine(const glm::mat4 &transform, const glm::vec3 &p0, const glm::vec3 &p1)
{
    glLoadMatrixf(glm::value_ptr(transform));
    glBegin(GL_LINES);
    glVertex3fv(glm::value_ptr(p0));
    glVertex3fv(glm::value_ptr(p1));
    glEnd();
}

void renderAxes(const glm::mat4 &transform, const glm::vec3 &worldCoord)
{
    // Render in NDC coordinates, no projection matrix needed
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    // ndc coord of axis center
    glm::vec3 ndcCoord = applyMatrix(getProjectionMatrix() * transform, worldCoord);

    glm::mat4 axisTransform = getProjectionMatrix() * transform;
    if (localMode)
        axisTransform = axisTransform * selectedJoint->worldTransform;

    glm::vec3 xdir = applyMatrix(axisTransform, glm::vec3(1,0,0), false);
    glm::vec3 ydir = applyMatrix(axisTransform, glm::vec3(0,1,0), false);
    glm::vec3 zdir = applyMatrix(axisTransform, glm::vec3(0,0,1), false);
    glm::vec3 p0   = glm::vec3(ndcCoord.x, ndcCoord.y, 0.f);
    glm::vec3 zp1  = ndcCoord + 0.5f * axisLength * zdir; zp1.z = 0.f;
    glm::vec3 xp1  = ndcCoord + 0.5f * axisLength * xdir; xp1.z = 0.f;
    glm::vec3 yp1  = ndcCoord + 0.5f * axisLength * ydir; yp1.z = 0.f;

    // render x axis
    glColor3f(1.0f, 0.5f, 0.5f);
    renderLine(glm::mat4(1.f),
            p0,
            xp1);
    // render y axis
    glColor3f(0.5f, 1.0f, 0.5f);
    renderLine(glm::mat4(1.f),
            p0,
            yp1);
    // render z axis
    glColor3f(0.5f, 0.5f, 1.0f);
    renderLine(glm::mat4(1.f),
            p0,
            zp1);
    // axis record NDC coordinates...
    axisNDC[0] = xp1;
    axisNDC[1] = yp1;
    axisNDC[2] = zp1;

    // render and record an enclosing circle
    glm::mat4 circleTransform = glm::scale(glm::translate(glm::mat4(1.f), ndcCoord), glm::vec3(circleRadius));
    glColor3f(1.f, 1.f, 1.f);
    renderCircle(circleTransform);
    // record center ndc coord
    circleNDC = ndcCoord;

    // Fix matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void renderScaleCircle(const glm::mat4 &transform, const glm::vec3 &worldCoord)
{
    // Render in NDC coordinates, no projection matrix needed
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    // ndc coord of axis center
    glm::vec3 ndcCoord = applyMatrix(getProjectionMatrix() * transform, worldCoord);

    // render and record an enclosing circle
    glm::mat4 circleTransform = glm::scale(glm::translate(glm::mat4(1.f), ndcCoord), glm::vec3(scaleCircleRadius));
    glColor3f(1.f, 0.5f, 0.5f);
    renderCircle(circleTransform);
    // record center ndc coord
    circleNDC = ndcCoord;

    // Fix matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

}

void renderCircle(const glm::mat4 &transform)
{
    const float r = 1.f;
    const int nsegments = 100;

    glLoadMatrixf(glm::value_ptr(transform));

    glBegin(GL_LINE_LOOP);
    for (float theta = 0.f; theta < 2*M_PI; theta += 2*M_PI/nsegments)
        glVertex3f(r*cosf(theta), r*sinf(theta), 0.f);
    glEnd();
}

void renderArc(const glm::vec3 &start, const glm::vec3 &center,
        const glm::vec3 &normal, float deg)
{
    glLoadIdentity();
    const int nsegments = 50;
    const glm::vec3 arm = start - center;

    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < nsegments+1; i++)
    {
        float theta = i * deg/nsegments - deg/2.f;
        glm::mat4 rot = glm::rotate(glm::mat4(1.f), theta, normal);
        glm::vec3 pt = applyMatrix(rot, arm) + center;
        glVertex3fv(glm::value_ptr(pt));
    }
    glEnd();
}

void renderRotationSphere(const glm::mat4 &transform, const glm::vec3 &worldCoord)
{
    // Render in NDC coordinates, no projection matrix needed
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    glm::mat4 fullMat = getProjectionMatrix() * transform;

    glm::vec3 ndcCoord = applyMatrix(fullMat, worldCoord);
    glm::vec3 screenCoord = glm::vec3(ndcCoord.x, ndcCoord.y, 0.f);

    // Render a white enclosing circle
    glColor3f(1,1,1);
    renderArc(glm::vec3(circleRadius,0,0)+screenCoord, screenCoord, glm::vec3(0,0,-1), 360.f);

    glm::mat4 axisTrans = transform;
    if (localMode)
        axisTrans = axisTrans * selectedJoint->worldTransform;
    glm::vec3 xdir = glm::normalize(applyMatrix(axisTrans, glm::vec3(1,0,0), false));
    glm::vec3 ydir = glm::normalize(applyMatrix(axisTrans, glm::vec3(0,1,0), false));
    glm::vec3 zdir = glm::normalize(applyMatrix(axisTrans, glm::vec3(0,0,1), false));

    glm::vec3 camdir(0,0,1);
    glm::vec3 dir; 
    
    glColor3f(1,0.5f,0.5f);
    dir = glm::normalize(camdir - glm::dot(camdir, xdir) * xdir);
    renderArc(screenCoord + axisLength*dir, screenCoord, xdir, 180.f);

    glColor3f(0.5f,1,0.5f);
    dir = glm::normalize(camdir - glm::dot(camdir, ydir) * ydir);
    renderArc(screenCoord + axisLength*dir, screenCoord, ydir, 180.f);

    glColor3f(0.5f,0.5f,1);
    dir = glm::normalize(camdir - glm::dot(camdir, zdir) * zdir);
    renderArc(screenCoord + axisLength*dir, screenCoord, zdir, 180.f);

    // Fix matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);


    // Record
    axisDir[0] = xdir;
    axisDir[1] = ydir;
    axisDir[2] = zdir;
}

void renderPoints(const glm::mat4 &transform, vert *verts, size_t nverts)
{
    glLoadMatrixf(glm::value_ptr(transform));

    glPointSize(2);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(4, GL_FLOAT, sizeof(vert), verts + offsetof(vert, pos));

    glDrawArrays(GL_POINTS, 0, nverts);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPointSize(1);
}

void renderSelectedPoints(const glm::mat4 &transform, rawmesh *mesh, int joint,
        const glm::vec4 &color)
{
    glLoadMatrixf(glm::value_ptr(transform));
    const size_t nverts = mesh->nverts;
    const vert* verts = mesh->verts;
    const int* joints = mesh->joints;
    const float* weights = mesh->weights;

    glPointSize(5);
    glBegin(GL_POINTS);
    for (size_t i = 0; i < nverts; i++)
        for (size_t j = 0; j < 4; j++)
            if (joints[4*i + j] == joint)
            {
                glColor4fv(glm::value_ptr(color * weights[4*i+j]));
                glVertex4fv(verts[i].pos);
            }
    glEnd();
    glPointSize(1);
}

void renderMesh(const glm::mat4 &transform, const vert_p4t2n3j8 *verts,
        size_t nverts)
{
    glLoadMatrixf(glm::value_ptr(transform));

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(4, GL_FLOAT, sizeof(vert_p4t2n3j8), &verts[0].pos);
    glDrawArrays(GL_TRIANGLES, 0, nverts);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void renderSkinnedMesh(const glm::mat4 &transform, const vert_p4t2n3j8 *verts,
        size_t nverts, const glm::vec4 &color)
{
    const std::vector<Joint*> joints = skeleton->getJoints();
    // First get a list of the necessary transformations
    std::vector<glm::mat4> jointMats(joints.size());
    for (size_t i = 0; i < joints.size(); i++)
    {
        jointMats[i] = joints[i]->worldTransform * joints[i]->inverseBindTransform;

        // for each transformation try to extract translation and rotation
        // TODO turn these into dual quats and use to skin using dual quat blending
        //glm::quat qrot  = glm::quat_cast(jointMats[i]);
        //glm::vec3 trans = applyMatrix(jointMats[i], glm::vec3(0.f));
        //std::cout << "(" << i << ") quat: " << qrot << " trans: " << trans << '\n';
    }
    glm::mat4 modelMatrix = glm::inverse(getViewMatrix()) * transform;

    // uniforms
    GLuint projectionUniform  = glGetUniformLocation(shader->program, "projectionMatrix");
    GLuint viewUniform        = glGetUniformLocation(shader->program, "viewMatrix");
    GLuint modelUniform       = glGetUniformLocation(shader->program, "modelMatrix");
    GLuint jointMatrixUniform = glGetUniformLocation(shader->program, "jointMatrices");
    GLuint colorUniform       = glGetUniformLocation(shader->program, "color");
    // attribs
    GLuint positionAttrib     = glGetAttribLocation(shader->program, "position");
    GLuint jointAttrib        = glGetAttribLocation(shader->program, "joint");
    GLuint weightAttrib       = glGetAttribLocation(shader->program, "weight");
    GLuint normalAttrib       = glGetAttribLocation(shader->program, "norm");
    GLuint coordAttrib        = glGetAttribLocation(shader->program, "texcoord");

    glUseProgram(shader->program);
    glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, glm::value_ptr(getProjectionMatrix()));
    glUniformMatrix4fv(viewUniform,  1, GL_FALSE, glm::value_ptr(getViewMatrix()));
    glUniformMatrix4fv(modelUniform,  1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniformMatrix4fv(jointMatrixUniform, jointMats.size(), GL_FALSE, &jointMats.front()[0][0]);
    glUniform4fv(colorUniform, 1, glm::value_ptr(color));

    glEnableVertexAttribArray(positionAttrib);
    glEnableVertexAttribArray(normalAttrib);
    glEnableVertexAttribArray(coordAttrib);
    glEnableVertexAttribArray(jointAttrib);
    glEnableVertexAttribArray(weightAttrib);

    glVertexAttribPointer(positionAttrib, 4, GL_FLOAT, GL_FALSE,
            sizeof(vert_p4t2n3j8), (char*)verts + offsetof(vert_p4t2n3j8, pos));
    glVertexAttribPointer(normalAttrib,   3, GL_FLOAT,  GL_FALSE,
            sizeof(vert_p4t2n3j8), (char*)verts + offsetof(vert_p4t2n3j8, norm));
    glVertexAttribPointer(coordAttrib,    2, GL_FLOAT,  GL_FALSE,
            sizeof(vert_p4t2n3j8), (char*)verts + offsetof(vert_p4t2n3j8, coord));
    glVertexAttribIPointer(jointAttrib,   4, GL_INT,
            sizeof(vert_p4t2n3j8), (char*)verts + offsetof(vert_p4t2n3j8, joints));
    glVertexAttribPointer(weightAttrib,   4, GL_FLOAT,  GL_FALSE,
            sizeof(vert_p4t2n3j8), (char*)verts + offsetof(vert_p4t2n3j8, weights));

    glDrawArrays(GL_TRIANGLES, 0, nverts);

    glDisableVertexAttribArray(positionAttrib);
    glDisableVertexAttribArray(normalAttrib);
    glDisableVertexAttribArray(coordAttrib);
    glDisableVertexAttribArray(jointAttrib);
    glDisableVertexAttribArray(weightAttrib);
    glUseProgram(0);
}

void renderTimeline()
{
    // Render in NDC coordinates, no projection matrix needed
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();


    for(int i = startFrame; i <= endFrame; i++)
    {
        // Highlight current frame red
        if (i == currentFrame)
            glColor3f(1.0f, 0.f, 0.f);
        else
            glColor3f(1.0f, 1.0f, 1.0f);
        if (keyframes.count(i) > 0)
            glColor3f(1.0f, 1.0f, 0.0f);
        glBegin(GL_LINES);
            glVertex3f(-1.0f + 2.f * i / (endFrame - startFrame + 1), 0.5f, 0);
            glVertex3f(-1.0f + 2.f * i / (endFrame - startFrame + 1), -0.5f, 0);
        glEnd();
        // std::cout << -1.0f + 2.f * i / (endFrame - startFrame) << std::endl;
    }

    // Foreground
    glColor3f(0.5f, 0.5f, 0.5f);
    glBegin(GL_QUADS);
        glVertex3f(-1, 0.5f, 0);
        glVertex3f(-1, -0.5f, 0);
        glVertex3f(1, -0.5f, 0);
        glVertex3f(1, 0.5f, 0);
    glEnd();


    // Background Color
    glColor3f(0.25f, 0.25f, 0.25f);
    glBegin(GL_QUADS);
        glVertex3f(-1, 1, 0);
        glVertex3f(-1, -1, 0);
        glVertex3f(1, -1, 0);
        glVertex3f(1, 1, 0);
    glEnd();

    // Fix matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void renderEditGrid()
{
    glLoadMatrixf(glm::value_ptr(getViewMatrix()));
    const int width = 16;

    glBegin(GL_LINES);
    for (int i = -width/2; i <= width/2; i++)
    {
        glColor3f(0.2f, 0.2f, 0.2f);
        // in zdirection
        if (i == 0) glColor3f(0.4f, 0.f, 0.f);
        glVertex3f(i, 0.f, -width/2);
        glVertex3f(i, 0.f, width/2);
        if (i == 0) glColor3f(0.f, 0.4f, 0.f);
        // in xdirection
        glVertex3f(-width/2, 0.f, i);
        glVertex3f(width/2, 0.f, i);
    }
    glEnd();
}

std::ostream& operator<< (std::ostream& os, const glm::vec2 &v)
{
    os << v.x << ' ' << v.y;
    return os;
}

std::ostream& operator<< (std::ostream& os, const glm::vec3 &v)
{
    os << v.x << ' ' << v.y << ' ' << v.z;
    return os;
}

std::ostream& operator<< (std::ostream& os, const glm::vec4 &v)
{
    os << v.x << ' ' << v.y << ' ' << v.z << ' ' << v.w;
    return os;
}

std::ostream& operator<< (std::ostream& os, const glm::quat &v)
{
    os << v.x << ' ' << v.y << ' ' << v.z << ' ' << v.w;
    return os;
}

// Quaternion operations follow...
glm::quat axisAngleToQuat(const glm::vec4 &a)
{
    /* 
     * qx = ax * sin(angle/2)
     * qy = ay * sin(angle/2)
     * qz = az * sin(angle/2)
     * qw = cos(angle/2)
     */

    float angle = M_PI/180.f * a.w;
    float sina = sinf(angle/2.f);
    float cosa = cosf(angle/2.f);

    // CAREFUL: w, x, y, z
    return glm::quat(cosa, a.x * sina, a.y * sina, a.z * sina);
}

glm::vec4 quatToAxisAngle(const glm::quat &q)
{
    float angle = 2.f * acosf(q.w);
    float sina2 = sinf(angle/2.f);
    angle *= 180.f/M_PI;

    if (sina2 == 0.f)
        return glm::vec4(0, 0, 1, angle);

    return glm::vec4(
            q.x / sina2,
            q.y / sina2,
            q.z / sina2,
            angle);
};

