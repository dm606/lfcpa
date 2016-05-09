#include "BuDDyInit.h"
#include "PointsToNode.h"

#define BUDDY_NODENUM 4000000
#define BUDDY_CACHESIZE 400000
#define BUDDY_MAXINCREASE 400000

BuDDyInit::BuDDyInit() {
    if (bdd_isrunning())
        return;

    bdd_init(BUDDY_NODENUM, BUDDY_CACHESIZE);
    bdd_setmaxincrease(BUDDY_MAXINCREASE);

    // Use three domains -- one for the left hand sides of pairs, one for the
    // right, and one for intermediate relations.
    int domains[3] = {MAX_NODE_COUNT, MAX_NODE_COUNT, MAX_NODE_COUNT};
    fdd_extdomain(domains, 3);

    PointsToNode::LeftToIntermediate = bdd_newpair();
    fdd_setpair(PointsToNode::LeftToIntermediate, LeftDomain, IntermediateDomain);
    PointsToNode::RightToIntermediate = bdd_newpair();
    fdd_setpair(PointsToNode::RightToIntermediate, RightDomain, IntermediateDomain);
    PointsToNode::RightToLeft = bdd_newpair();
    fdd_setpair(PointsToNode::RightToLeft, RightDomain, LeftDomain);
    PointsToNode::SummaryNodesBDD = bdd_false();
}

