#include "ClippingMesh.h"


ClippingMesh::~ClippingMesh() {

    mVerts.clear();
    mVerts.shrink_to_fit();

    mEdges.clear();
    mEdges.shrink_to_fit();

    mFaces.clear();
    mFaces.shrink_to_fit();
}


void ClippingMesh::initialize() {

    std::cout << "\ninitializing ClippingMesh... ";

    std::cout << "Done!\n";
}



bool ClippingMesh::clipMesh(SimpleMesh *sm) {

    mHalfEdgeMesh->printMesh();

    unsigned int numSplittingPlanes = mHalfEdgeMesh->getCompound()->getNumberOfSplittingPlanes();

    Vector3<float> refVoronoiPoint = mHalfEdgeMesh->getVoronoiPoint(0);

    // Loop over all faces of our mesh
    for(unsigned int i = 0; i < mHalfEdgeMesh->getNumFaces(); i++) {

        Polygon polygon;

        // Collect all vertices from the face that we want to clip
        polygon.v0 = mHalfEdgeMesh->getVert(mHalfEdgeMesh->getEdge(mHalfEdgeMesh->getFace(i).edge).vert).pos;
        polygon.v1 = mHalfEdgeMesh->getVert(mHalfEdgeMesh->getEdge(mHalfEdgeMesh->getEdge(mHalfEdgeMesh->getFace(i).edge).next).vert).pos;
        polygon.v2 = mHalfEdgeMesh->getVert(mHalfEdgeMesh->getEdge(mHalfEdgeMesh->getEdge(mHalfEdgeMesh->getEdge(mHalfEdgeMesh->getFace(i).edge).next).next).vert).pos;

        Vector3<float> correctNormal = Cross(polygon.v1 - polygon.v0, polygon.v2 - polygon.v0).Normalize();

        polygon.normal = correctNormal;

        std::cout << "\nPolygon verts: " << polygon.v0 << ", " << polygon.v1 << ", " << polygon.v2 << std::endl;

        // For now the polygon contains 3 vertices
        polygon.numVerts = 3;

        // Loop over all splitting planes for our mesh
        for(unsigned int j = 0; j < 1; j++) {

            std::pair<Vector3<float>, Vector3<float> > voronoiPair = mHalfEdgeMesh->getCompound()->getSplittingPlane(j)->getVoronoiPoints();

            // Normal of the splitting plane, we can get it from SplittingPlane object,
            // but then it might point in the wrong direction so computing it should be faster
            Vector3<float> normal;
            if(voronoiPair.first == refVoronoiPoint)
                normal = (voronoiPair.second - voronoiPair.first).Normalize();
            else
                normal = (voronoiPair.first - voronoiPair.second).Normalize();

            // Arbitrary point on the splitting plane (any vertex), in this case the first vertex
            Vector3<float> P = mHalfEdgeMesh->getCompound()->getSplittingPlane(j)->getVertex(0);

            // Clip the polygon and set the number of vertices
            polygon.numVerts = clipFace(polygon, normal, P);

            /*************************************************************************
    
            Determine which vertices that form the new polygon along the splitting plane.
            This polygon needs to be triangulated and have the correct orientation.
            Fix so we can loop over all splitting planes and only compute the splitting for
            planes that are created from the voronoipoint that corresponds to the current cell.

            *************************************************************************/
        }

        sortPolygonCounterClockWise(polygon);

        std::cout << "\n----- NEW FACE -----\n" << std::endl;

        std::vector<Vector3<float> > face1;
        std::vector<Vector3<float> > face2;

        if(polygon.numVerts == 4) {
            
            Polygon P2;
            triangulateQuad(polygon, P2);
            P2.normal = Cross(polygon.v2 - polygon.v0, polygon.v1 - polygon.v0).Normalize();
            checkTriangleOrientation(P2, correctNormal);
            face1.push_back(P2.v0);
            face1.push_back(P2.v1);
            face1.push_back(P2.v2);
            sm->addFace(face1);

            std::cout << "\nPolygon " << i << " :\n";
            std::cout << "\nv0: " << P2.v0 << "\nv1: " << P2.v1 << "\nv2: " << P2.v2 << std::endl;
            std::cout << "numverts: " << P2.numVerts << std::endl;

        }

        if(polygon.numVerts != 0 && polygon.numVerts != -1) {
         
            checkTriangleOrientation(polygon, correctNormal);
            face2.push_back(polygon.v0);
            face2.push_back(polygon.v1);
            face2.push_back(polygon.v2);
            sm->addFace(face2);

            std::cout << "\nPolygon " << i << " :\n";
            std::cout << "\nv0: " << polygon.v0 << "\nv1: " << polygon.v1 << "\nv2: " << polygon.v2 << std::endl;
            std::cout << "numverts: " << polygon.numVerts << std::endl;
        }

        face1.clear();
        face2.clear();
        face1.shrink_to_fit();
        face2.shrink_to_fit();
    }
}


int ClippingMesh::clipFace(Polygon &p, Vector3<float> n, Vector3<float> p0) {
    
    double A, B, C, D;
    double l;
    double side[3];
    Vector3<float> q;

    //Determine the equation of the plane as
    //Ax + By + Cz + D = 0
  
    l = sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
    A = (double)n[0] / l;
    B = (double)n[1] / l;
    C = (double)n[2] / l;
    D = -((double)n[0]*(double)p0[0] + (double)n[1]*(double)p0[1] + (double)n[2]*(double)p0[2]);

    // Evaluate the equation of the plane for each vertex
    // If side < 0 then it is on the side to be retained
    // else it is to be clippped

    side[0] = A*(double)p.v0[0] + B*(double)p.v0[1] + C*(double)p.v0[2] + D;
    side[1] = A*(double)p.v1[0] + B*(double)p.v1[1] + C*(double)p.v1[2] + D;
    side[2] = A*(double)p.v2[0] + B*(double)p.v2[1] + C*(double)p.v2[2] + D;

    // Are all the vertices are on the clipped side
    if (side[0] >= 0 && side[1] >= 0 && side[2] >= 0) {
        std::cout << "--- Clipping 1 ---" << std::endl;
        return(0);
   }

    // Are all the vertices on the not-clipped side
    if (side[0] <= 0 && side[1] <= 0 && side[2] <= 0) {
        std::cout << "--- Clipping 2 ---" << std::endl;
        return(3);
    }

    // Is p0 the only point on the clipped side
    if (side[0] > 0 && side[1] < 0 && side[2] < 0) {
       q[0] = p.v0[0] - side[0] * (p.v2[0] - p.v0[0]) / (side[2] - side[0]);
       q[1] = p.v0[1] - side[0] * (p.v2[1] - p.v0[1]) / (side[2] - side[0]);
       q[2] = p.v0[2] - side[0] * (p.v2[2] - p.v0[2]) / (side[2] - side[0]);
       p.v3 = q;
       q[0] = p.v0[0] - side[0] * (p.v1[0] - p.v0[0]) / (side[1] - side[0]);
       q[1] = p.v0[1] - side[0] * (p.v1[1] - p.v0[1]) / (side[1] - side[0]);
       q[2] = p.v0[2] - side[0] * (p.v1[2] - p.v0[2]) / (side[1] - side[0]);
       p.v0 = q;

       std::cout << "--- Clipping 3 ---" << std::endl;

       return(4);
    }

    // Is p1 the only point on the clipped side 
    if (side[1] > 0 && side[0] < 0 && side[2] < 0) {
       p.v3 = p.v2;
       q[0] = p.v1[0] - side[1] * (p.v2[0] - p.v1[0]) / (side[2] - side[1]);
       q[1] = p.v1[1] - side[1] * (p.v2[1] - p.v1[1]) / (side[2] - side[1]);
       q[2] = p.v1[2] - side[1] * (p.v2[2] - p.v1[2]) / (side[2] - side[1]);
       p.v2 = q;
       q[0] = p.v1[0] - side[1] * (p.v0[0] - p.v1[0]) / (side[0] - side[1]);
       q[1] = p.v1[1] - side[1] * (p.v0[1] - p.v1[1]) / (side[0] - side[1]);
       q[2] = p.v1[2] - side[1] * (p.v0[2] - p.v1[2]) / (side[0] - side[1]);
       p.v1 = q;

       std::cout << "--- Clipping 4 ---" << std::endl;

       return(4);
    }

    // Is p2 the only point on the clipped side
    if (side[2] > 0 && side[0] < 0 && side[1] < 0) {
       q[0] = p.v2[0] - side[2] * (p.v0[0] - p.v2[0]) / (side[0] - side[2]);
       q[1] = p.v2[1] - side[2] * (p.v0[1] - p.v2[1]) / (side[0] - side[2]);
       q[2] = p.v2[2] - side[2] * (p.v0[2] - p.v2[2]) / (side[0] - side[2]);
       p.v3 = q;
       q[0] = p.v2[0] - side[2] * (p.v1[0] - p.v2[0]) / (side[1] - side[2]);
       q[1] = p.v2[1] - side[2] * (p.v1[1] - p.v2[1]) / (side[1] - side[2]);
       q[2] = p.v2[2] - side[2] * (p.v1[2] - p.v2[2]) / (side[1] - side[2]);
       p.v2 = q;

       std::cout << "--- Clipping 5 ---" << std::endl;

       return(4);
    }

    // Is p0 the only point on the not-clipped side
    if (side[0] < 0 && side[1] > 0 && side[2] > 0) {
       q[0] = p.v0[0] - side[0] * (p.v1[0] - p.v0[0]) / (side[1] - side[0]);
       q[1] = p.v0[1] - side[0] * (p.v1[1] - p.v0[1]) / (side[1] - side[0]);
       q[2] = p.v0[2] - side[0] * (p.v1[2] - p.v0[2]) / (side[1] - side[0]);
       p.v1 = q;
       q[0] = p.v0[0] - side[0] * (p.v2[0] - p.v0[0]) / (side[2] - side[0]);
       q[1] = p.v0[1] - side[0] * (p.v2[1] - p.v0[1]) / (side[2] - side[0]);
       q[2] = p.v0[2] - side[0] * (p.v2[2] - p.v0[2]) / (side[2] - side[0]);
       p.v2 = q;

       std::cout << "--- Clipping 6 ---" << std::endl;

       return(3);
    }

    // Is p1 the only point on the not-clipped side
    if (side[1] < 0 && side[0] > 0 && side[2] > 0) {
       q[0] = p.v1[0] - side[1] * (p.v0[0] - p.v1[0]) / (side[0] - side[1]);
       q[1] = p.v1[1] - side[1] * (p.v0[1] - p.v1[1]) / (side[0] - side[1]);
       q[2] = p.v1[2] - side[1] * (p.v0[2] - p.v1[2]) / (side[0] - side[1]);
       p.v0 = q;
       q[0] = p.v1[0] - side[1] * (p.v2[0] - p.v1[0]) / (side[2] - side[1]);
       q[1] = p.v1[1] - side[1] * (p.v2[1] - p.v1[1]) / (side[2] - side[1]);
       q[2] = p.v1[2] - side[1] * (p.v2[2] - p.v1[2]) / (side[2] - side[1]);
       p.v2 = q;

       std::cout << "--- Clipping 7 ---" << std::endl;

       return(3);
    }

    // Is p2 the only point on the not-clipped side
    if (side[2] < 0 && side[0] > 0 && side[1] > 0) {
       q[0] = p.v2[0] - side[2] * (p.v1[0] - p.v2[0]) / (side[1] - side[2]);
       q[1] = p.v2[1] - side[2] * (p.v1[1] - p.v2[1]) / (side[1] - side[2]);
       q[2] = p.v2[2] - side[2] * (p.v1[2] - p.v2[2]) / (side[1] - side[2]);
       p.v1 = q;
       q[0] = p.v2[0] - side[2] * (p.v0[0] - p.v2[0]) / (side[0] - side[2]);
       q[1] = p.v2[1] - side[2] * (p.v0[1] - p.v2[1]) / (side[0] - side[2]);
       q[2] = p.v2[2] - side[2] * (p.v0[2] - p.v2[2]) / (side[0] - side[2]);
       p.v0 = q;

       std::cout << "--- Clipping 8 ---" << std::endl;

       return(3);
    }

    // Shouldn't get here
    return(-1);
}


bool ClippingMesh::sortPolygonCounterClockWise(Polygon &P) {

    // If the polygon has no vertices, then return it as sorted
    if(P.numVerts == 0)
        return true;

    Vector3<float> centerOfMass;

    if(P.numVerts == 3) {
        centerOfMass = P.v0 + P.v1 + P.v2;
    } else if(P.numVerts == 4) {
        centerOfMass = P.v0 + P.v1 + P.v2 + P.v3;
    }

    centerOfMass /= static_cast<float>(P.numVerts);

    unsigned int s, t;

    if(P.numVerts == 3 || P.numVerts == 4) {

        // Determine projection plane
        if((P.v0[0] > P.v1[0] - EPSILON_2 && P.v0[0] < P.v1[0] + EPSILON_2) && (P.v0[0] > P.v2[0] - EPSILON_2 && P.v0[0] < P.v2[0] + EPSILON_2)) {
            s = 1;
            t = 2;
        } else if((P.v0[1] > P.v1[1] - EPSILON_2 && P.v0[1] < P.v1[1] + EPSILON_2) && (P.v0[1] > P.v2[1] - EPSILON_2 && P.v0[1] < P.v2[1] + EPSILON_2)) {
            s = 0;
            t = 2;
        } else {
            s = 0;
            t = 1;
        }

        std::vector<std::pair<float, Vector3<float> > > verts;

        // Compute angles for each vertex, the static_cast call is just 
        // to ensure that we get a float back from the angle computation
        Vector3<float> v = P.v0 - centerOfMass;
        verts.push_back(std::make_pair(static_cast<float>(atan2(v[s], v[t])), P.v0));

        v = P.v1 - centerOfMass;
        verts.push_back(std::make_pair(static_cast<float>(atan2(v[s], v[t])), P.v1));

        v = P.v2 - centerOfMass;
        verts.push_back(std::make_pair(static_cast<float>(atan2(v[s], v[t])), P.v2));

        if(P.numVerts == 4) {
            v = P.v3 - centerOfMass;
            verts.push_back(std::make_pair(static_cast<float>(atan2(v[s], v[t])), P.v3));
        }

        // Sort verts by angle
        std::sort(
        verts.begin(), 
        verts.end(), 
        [](const std::pair<float, Vector3<float> > p1, const std::pair<float, Vector3<float> > p2) { 
            return p1.first < p2.first; 
        } );

        P.v0 = verts[0].second;
        P.v1 = verts[1].second;
        P.v2 = verts[2].second;

        if(P.numVerts == 4)
            P.v3 = verts[3].second;

        return true;
    }

    // It's impossible to get here
    return false;
}


bool ClippingMesh::triangulateQuad(Polygon &P1, Polygon &P2) {

    P2.v0 = P1.v0;
    P2.v1 = P1.v2;
    P2.v2 = P1.v3;

    P1.numVerts = 3;
    P2.numVerts = 3;
}


bool ClippingMesh::checkTriangleOrientation(Polygon &P, Vector3<float> normal) {

    Vector3<float> edge1 = P.v1 - P.v0;
    Vector3<float> edge2 = P.v2 - P.v0;

    Vector3<float> polygonNormal = Cross(edge1, edge2).Normalize();

    if(normal == polygonNormal) {
        std::cout << "\nSAMMA NORMAL" << std::endl;
        std::cout << "correct normal: " << P.normal << std::endl;
        std::cout << "this normal: " << polygonNormal << std::endl;
    } else {
        std::cout << "\nOLIKA NORMAL" << std::endl;
        std::cout << "correct normal: " << P.normal << std::endl;
        std::cout << "this normal: " << polygonNormal << std::endl;

        Vector3<float> tmp = P.v1;
        P.v1 = P.v2;
        P.v2 = tmp;
        P.normal = Cross(P.v1 - P.v0, P.v2 - P.v0).Normalize();
        std::cout << "fixed normal: " << P.normal << std::endl;
    }
}

