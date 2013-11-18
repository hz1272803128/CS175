#include "debug.h"

/* Some constant shits */
static const int    g_numShells = 24;
static       double g_furHeight = 0.21;
static       double g_hairyness = 0.7;
static const Cvec3  g_gravity(0, -0.5, 0);
static       double g_timeStep = 0.02;
static       double g_numStepsPerFrame = 10;
static       double g_damping = 0.96;
static       double g_stiffness = 4;

/** Bunny node */ // useful comment
static shared_ptr<SgRbtNode> g_bunnyNode;

/** The bunny mesh */
static Mesh g_bunnyMesh;
/** Shell geometries */
static vector<shared_ptr<SimpleGeometryPNX> > g_bunnyShellGeometries;

/** Used for physical simulation */
static int g_simulationsPerSecond = 60;

/** The hair tip position in world-space coordinates */
static std::vector<Cvec3> g_tipPos;
/** The hair tip velocity in world-space coordinates */
static std::vector<Cvec3> g_tipVelocity;

/**
 * Given a vertex on the bunny, returns the at-rest position of the hair tip.
 * @param  v The vertex on the bunny.
 * @return   The position of the tip of the hair.
 */
Cvec3 getAtRestTipPosition(Mesh::Vertex v) {
  return v.getNormal() * g_furHeight;
}

/**
 * Sets the tip positions to be the tips of the bunny's hair positions. Sets the
 * velocities to be initialized to zero.
 * @param mesh The bunny mesh.
 */
void initializeBunnyPhysics(Mesh &mesh) {
  for (int i = 0; i < mesh.getNumVertices(); i++) {
    g_tipPos.push_back(getAtRestTipPosition(mesh.getVertex(i)));
    g_tipVelocity.push_back(Cvec3());
  }
}

/**
 * Computes the vertex on a bunny shell.
 *
 * For each vertex with position p, compute the longest hair position s
 * Compute n = (s - p) / g_numShells
 * Compute our specific vertex position with p + n * layer
 *
 * @param       v The vertex on the bunny itself.
 * @param       i The layer of the bunny that we're computing.
 * @param vertNum The number of the vertex we're currently doing. 0, 1, or 2.
 */
static VertexPNX computeHairVertex(Mesh::Vertex v, int i, int vertNum, RigTForm bunnyRbt) {
  return VertexPNX(
    v.getPosition() + (
      Cvec3(inv(bunnyRbt) * Cvec4(g_tipPos[v.getIndex()], 1))
      / g_numShells) * i,
    v.getNormal(),
    Cvec2(vertNum == 1 ? g_hairyness : 0, vertNum == 2 ? g_hairyness : 0)
  );
}

// TODO: BLATANT HACK MUST BE FIXED
int useRbt = 0;
/**
 * Returns the vertices for the layer-th layer of the bunny shell.
 */
static vector<VertexPNX> getBunnyShellGeometryVertices(Mesh &mesh, int layer) {
  vector<VertexPNX> vs;
  // cout << "here mother fucker " << endl;
  RigTForm bunnyRbt;
  if (useRbt > 0) {
    cout << "uSING THIS " << endl;
    bunnyRbt = getPathAccumRbt(g_world, g_bunnyNode);
  } else {
    bunnyRbt = RigTForm();
  }
  // printRigTForm(bunnyRbt);
  // cout << "here father shitter " << endl;
  /* For each face: */
  for (int i = 0; i < mesh.getNumFaces(); i++) {
    Mesh::Face f = mesh.getFace(i);
    /* For each vertex of each face: */
    for (int j = 1; j < f.getNumVertices() - 1; j++) {
      vs.push_back(computeHairVertex(f.getVertex(  0), layer, 0, bunnyRbt));
      vs.push_back(computeHairVertex(f.getVertex(  j), layer, 1, bunnyRbt));
      vs.push_back(computeHairVertex(f.getVertex(j+1), layer, 2, bunnyRbt));
    }
  }

  return vs;
}

static void updateHairCalculation(
    Mesh::Vertex vec,
    int vertexIndex,
    RigTForm bunnyRbt,
    RigTForm invBunnyRbt) {
  // /* Reassignments so that we're consistent with notation in the assignment. */
  // double T = g_timeStep;
  // Cvec3 p = Cvec3(invBunnyRbt * Cvec4(vec.getPosition(), 1));
  // Cvec3 s = Cvec3(invBunnyRbt * Cvec4(getAtRestTipPosition(vec), 1));
  // Cvec3 t = Cvec3(invBunnyRbt * Cvec4(g_tipPos[vertexIndex], 1));
  // Cvec3 v = Cvec3(invBunnyRbt * Cvec4(g_tipVelocity[vertexIndex], 1));

  // /* Step 1: Compute f */
  // Cvec3 f = g_gravity + (s - t) * g_stiffness;
  // /* Step 2: Update t */
  // t = t + v * T;
  // /* Step 3: Constrain t */
  // g_tipPos[vertexIndex] = Cvec3(bunnyRbt * Cvec4(p + (t - p) / norm(t - p) * g_furHeight), 1);
  // /* Step 4: Update v */
  // g_tipVelocity[vertexIndex] = Cvec3(bunnyRbt * Cvec4((v + f * T) * g_damping), 1);



  Cvec3 p = Cvec3(invBunnyRbt * Cvec4(vec.getPosition(), 1));
  Cvec3 newp = p + Cvec3(0, 1, 0);
  g_tipPos[vertexIndex] = Cvec3(0, -1, 0);
}

/**
 * Updates the hair calculations for the bunny based on the physics simulation
 * descriptions provided in the assignment.
 */
static void updateHairs(Mesh &mesh) {
  RigTForm bunnyRbt = getPathAccumRbt(g_world, g_bunnyNode);
  RigTForm invBunnyRbt = inv(bunnyRbt);
  for (int i = 0; i < mesh.getNumVertices(); i++) {
    Mesh::Vertex v = mesh.getVertex(i);
    updateHairCalculation(v, i, bunnyRbt, invBunnyRbt);
  }

  // TODO: EVERYTHING BELOW SHOULD ONLY BE DONE ONCE PER RENDER CYCLE
  for (int i = 0; i < g_numShells; ++i) {
    vector<VertexPNX> verticies = getBunnyShellGeometryVertices(g_bunnyMesh, i);
    g_bunnyShellGeometries[i]->upload(&verticies[0], verticies.size());
  }
}

/**
 * Performs dynamics simulation g_simulationsPerSecond times per second
 */
static void hairsSimulationCallback(int _) {
  printVector("Tip vertex: ", g_tipPos[0]);
  /* Update the hair dynamics. HACK: Ideally, we'd be passing in g_bunnyMesh to
     this function, but that's hard since it's a fucking callback. */
  updateHairs(g_bunnyMesh);
  /* Schedule this to get called again */
  glutTimerFunc(1250 / g_simulationsPerSecond, hairsSimulationCallback, _);
  /* Force visual refresh */
  glutPostRedisplay();
}