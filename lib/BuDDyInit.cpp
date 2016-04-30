#include "BuDDyInit.h"
#include "PointsToNode.h"

BuDDyInit::BuDDyInit() {
    if (bdd_isrunning())
        return;

    // TODO: Tune
    bdd_init(1000, 1000);
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