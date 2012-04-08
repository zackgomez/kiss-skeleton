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
static void renderTimeline();

void redraw(void)
{
    // Render main window
    glViewport(0, timelineHeight, windowWidth, windowHeight - timelineHeight);

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

