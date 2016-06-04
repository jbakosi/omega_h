static void refine_edge_interiors(
    Mesh& mesh,
    LOs keys2edges,
    LOs keys2midverts,
    LOs old_verts2new_verts,
    LOs& keys2cuts,
    LOs& keys2pairs,
    LOs& cut_verts2verts_r,
    LOs& pair_verts2verts_r) {
  auto edge_verts2verts = mesh.ask_verts_of(EDGE);
  auto nkeys = keys2edges.size();
  auto ndoms = nkeys;
  auto npairs = ndoms * 2;
  Write<LO> pair_verts2verts(npairs * 2);
  auto f = OSH_LAMBDA(LO key) {
    auto edge = keys2edges[key];
    auto midvert = keys2midverts[key];
    pair_verts2verts[key * 4 + 0] =
      old_verts2new_verts[edge_verts2verts[edge * 2 + 0]];
    pair_verts2verts[key * 4 + 1] = midvert;
    pair_verts2verts[key * 4 + 2] = midvert;
    pair_verts2verts[key * 4 + 3] =
      old_verts2new_verts[edge_verts2verts[edge * 2 + 1]];
  };
  parallel_for(nkeys, f);
  keys2cuts = LOs(nkeys + 1, 0, 1);
  keys2pairs = LOs(nkeys + 1, 0, 2);
  cut_verts2verts_r = keys2midverts;
  pair_verts2verts_r = pair_verts2verts;
}

/* "domains" (doms for short) are the (dim)-dimensional
   entities adjacent to a key edge.
   each one of them is split into two "pair" entities
   and a "cut" entity running down the middle.
   when (dim == 1), the "cut" entities are vertices
   so some special handling is needed in that case
   (see refine_edge_interiors() above).

   both pairs and cuts are part of the general set of
   "product" entities (prods for short), i.e. new cavity
   interior entities created during mesh modification.
 */

void refine_domain_interiors(
    Mesh& mesh,
    Int dim,
    LOs keys2edges,
    LOs keys2midverts,
    LOs old_verts2new_verts,
    LOs& keys2cuts,
    LOs& keys2pairs,
    LOs& cut_verts2verts_r,
    LOs& pair_verts2verts_r) {
  if (dim == 1) {
    refine_edge_interiors(mesh, keys2edges, keys2midverts, old_verts2new_verts,
        keys2cuts, keys2pairs, cut_verts2verts_r, pair_verts2verts_r);
    return;
  }
  auto nkeys = keys2edges.size();
  auto edge_verts2verts = mesh.ask_verts_of(EDGE);
  auto dom_verts2verts = mesh.ask_verts_of(dim);
  auto edges2doms = mesh.ask_up(EDGE, dim);
  auto edges2edge_doms = edges2doms.a2ab;
  auto edge_doms2doms = edges2doms.ab2b;
  auto edge_dom_codes = edges2doms.codes;
  auto edge_dom_degrees = get_degrees(edges2edge_doms);
  auto key_dom_degrees = unmap(keys2edges, edge_dom_degrees, 1);
  auto keys2key_doms = offset_scan(key_dom_degrees);
  auto ndoms = keys2key_doms.last();
  auto ncuts = ndoms;
  auto npairs = ndoms * 2;
  keys2cuts = keys2key_doms;
  keys2pairs = multiply_each_by(2, keys2key_doms);
  Write<LO> cut_verts2verts(ncuts * (dim));
  Write<LO> pair_verts2verts(npairs * (dim + 1));
  auto f = OSH_LAMBDA(LO key) {
    auto edge = keys2edges[key];
    auto midvert = keys2midverts[key];
    auto pair = keys2pairs[key];
    auto cut = keys2cuts[key];
    for (auto edge_dom = edges2edge_doms[edge];
         edge_dom < edges2edge_doms[edge + 1];
         ++edge_dom) {
      auto dom = edge_doms2doms[edge_dom];
      auto code = edge_dom_codes[edge_dom];
      auto dde = code_which_down(code);
      auto rot = code_rotation(code);
      auto ddv2v = &dom_verts2verts[dom * (dim + 1)];
      for (Int eev = 0; eev < 2; ++eev) {
        /* a new cell is formed from an old cell by finding
           its side that is opposite to one of the edge endpoints
           and connecting it to the midpoint to form the new cell */
        auto dev = eev ^ rot;
        auto ddv = down_templates[dim][EDGE][dde][dev];
        auto dds = opposite_templates[dim][VERT][ddv];
        auto ppv2v = &pair_verts2verts[pair * (dim + 1)];
        for (Int dsv = 0; dsv < dim; ++dsv) {
          auto ddv2 = down_templates[dim][dim - 1][dds][dsv];
          auto ov = ddv2v[ddv2];
          auto nv = old_verts2new_verts[ov];
          ppv2v[dsv] = nv;
        }
        ppv2v[dim] = midvert;
        ++pair;
      }
      /* a "cut" is formed by connecting its opposite
         "tip" (running out of words) to the new midpoint
         vertex. for triangle domains, the tip is the vertex
         not adjacent to the key edge. for tet domains, the tip
         is the edge not adjacent to the key edge. */
      auto ccv2v = &cut_verts2verts[cut * dim];
      auto ddt = opposite_templates[dim][EDGE][dde];
      for (Int dtv = 0; dtv < dim - 1; ++dtv) {
        auto ddv2 = down_templates[dim][dim - 2][ddt][dtv];
        auto ov = ddv2v[ddv2];
        auto nv = old_verts2new_verts[ov];
        ccv2v[dtv] = nv;
      }
      ccv2v[dim - 1] = midvert;
      ++cut;
    }
  };
  parallel_for(nkeys, f);
  cut_verts2verts_r = cut_verts2verts;
  pair_verts2verts_r = pair_verts2verts;
}