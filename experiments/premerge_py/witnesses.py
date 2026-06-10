# Fixed G_A (notes B.5): anchors S={0,1,2,3}, private {100,101,102,103}, two modules.
GA_EDGES = [(0,1),(0,100),(1,101),(100,101),(0,101),
            (2,3),(2,102),(3,103),(102,103),(2,103),
            (1,2)]                         # bridge between the two A-modules
GA_NODES = [0,1,2,3,100,101,102,103]
GA_PART  = {0:0,1:0,100:0,101:0, 2:1,3:1,102:1,103:1}
S = [0,1,2,3]

def _ring(nodes):
    return [(nodes[i], nodes[(i+1) % len(nodes)]) for i in range(len(nodes))]

_PRIV = [200,201,202,203,204,205,206,207]
_ANCHOR_LINKS = [(200,0),(207,3)]          # identical seam in both versions

def partner_v1():
    """One 8-cycle over the private nodes."""
    EB = _ring(_PRIV) + _ANCHOR_LINKS
    VB = sorted(set(_PRIV) | set(S))
    partB = {n: 100 for n in _PRIV}
    return EB, VB, partB

def partner_v2():
    """Two disjoint 4-cycles over the same private nodes."""
    EB = _ring(_PRIV[:4]) + _ring(_PRIV[4:]) + _ANCHOR_LINKS
    VB = sorted(set(_PRIV) | set(S))
    partB = {**{n:100 for n in _PRIV[:4]}, **{n:101 for n in _PRIV[4:]}}
    return EB, VB, partB
