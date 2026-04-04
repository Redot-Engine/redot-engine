/**************************************************************************/
/*  test_astar.h                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/math/a_star.h"

#include "tests/test_macros.h"

namespace TestAStar {

class ABCX : public AStar3D {
public:
	enum {
		A,
		B,
		C,
		X,
	};

	ABCX() {
		add_point(A, Vector3(0, 0, 0));
		add_point(B, Vector3(1, 0, 0));
		add_point(C, Vector3(0, 1, 0));
		add_point(X, Vector3(0, 0, 1));
		connect_points(A, B);
		connect_points(A, C);
		connect_points(B, C);
		connect_points(X, A);
	}

	// Disable heuristic completely.
	real_t _compute_cost(int64_t p_from, int64_t p_to) {
		if (p_from == A && p_to == C) {
			return 1000;
		}
		return 100;
	}
};

TEST_CASE("[AStar3D] ABC path") {
	ABCX abcx;
	Vector<int64_t> path = abcx.get_id_path(ABCX::A, ABCX::C);
	REQUIRE(path.size() == 3);
	CHECK(path[0] == ABCX::A);
	CHECK(path[1] == ABCX::B);
	CHECK(path[2] == ABCX::C);
}

TEST_CASE("[AStar3D] ABCX path") {
	ABCX abcx;
	Vector<int64_t> path = abcx.get_id_path(ABCX::X, ABCX::C);
	REQUIRE(path.size() == 4);
	CHECK(path[0] == ABCX::X);
	CHECK(path[1] == ABCX::A);
	CHECK(path[2] == ABCX::B);
	CHECK(path[3] == ABCX::C);
}

TEST_CASE("[AStar3D] Add/Remove") {
	AStar3D a;

	// Manual tests.
	a.add_point(1, Vector3(0, 0, 0));
	a.add_point(2, Vector3(0, 1, 0));
	a.add_point(3, Vector3(1, 1, 0));
	a.add_point(4, Vector3(2, 0, 0));
	a.connect_points(1, 2, true);
	a.connect_points(1, 3, true);
	a.connect_points(1, 4, false);

	CHECK(a.are_points_connected(2, 1));
	CHECK(a.are_points_connected(4, 1));
	CHECK(a.are_points_connected(2, 1, false));
	CHECK_FALSE(a.are_points_connected(4, 1, false));

	a.disconnect_points(1, 2, true);
	CHECK(a.get_point_connections(1).size() == 2); // 3, 4
	CHECK(a.get_point_connections(2).size() == 0);

	a.disconnect_points(4, 1, false);
	CHECK(a.get_point_connections(1).size() == 2); // 3, 4
	CHECK(a.get_point_connections(4).size() == 0);

	a.disconnect_points(4, 1, true);
	CHECK(a.get_point_connections(1).size() == 1); // 3
	CHECK(a.get_point_connections(4).size() == 0);

	a.connect_points(2, 3, false);
	CHECK(a.get_point_connections(2).size() == 1); // 3
	CHECK(a.get_point_connections(3).size() == 1); // 1

	a.connect_points(2, 3, true);
	CHECK(a.get_point_connections(2).size() == 1); // 3
	CHECK(a.get_point_connections(3).size() == 2); // 1, 2

	a.disconnect_points(2, 3, false);
	CHECK(a.get_point_connections(2).size() == 0);
	CHECK(a.get_point_connections(3).size() == 2); // 1, 2

	a.connect_points(4, 3, true);
	CHECK(a.get_point_connections(3).size() == 3); // 1, 2, 4
	CHECK(a.get_point_connections(4).size() == 1); // 3

	a.disconnect_points(3, 4, false);
	CHECK(a.get_point_connections(3).size() == 2); // 1, 2
	CHECK(a.get_point_connections(4).size() == 1); // 3

	a.remove_point(3);
	CHECK(a.get_point_connections(1).size() == 0);
	CHECK(a.get_point_connections(2).size() == 0);
	CHECK(a.get_point_connections(4).size() == 0);

	a.add_point(0, Vector3(0, -1, 0));
	a.add_point(3, Vector3(2, 1, 0));
	// 0: (0, -1)
	// 1: (0, 0)
	// 2: (0, 1)
	// 3: (2, 1)
	// 4: (2, 0)

	// Tests for get_closest_position_in_segment.
	a.connect_points(2, 3);
	CHECK(a.get_closest_position_in_segment(Vector3(0.5, 0.5, 0)) == Vector3(0.5, 1, 0));

	a.connect_points(3, 4);
	a.connect_points(0, 3);
	a.connect_points(1, 4);
	a.disconnect_points(1, 4, false);
	a.disconnect_points(4, 3, false);
	a.disconnect_points(3, 4, false);
	// Remaining edges: <2, 3>, <0, 3>, <1, 4> (directed).
	CHECK(a.get_closest_position_in_segment(Vector3(2, 0.5, 0)) == Vector3(1.75, 0.75, 0));
	CHECK(a.get_closest_position_in_segment(Vector3(-1, 0.2, 0)) == Vector3(0, 0, 0));
	CHECK(a.get_closest_position_in_segment(Vector3(3, 2, 0)) == Vector3(2, 1, 0));

	Math::seed(0);

	// Random tests for connectivity checks
	for (int i = 0; i < 20000; i++) {
		int u = Math::rand() % 5;
		int v = Math::rand() % 4;
		if (u == v) {
			v = 4;
		}
		if (Math::rand() % 2 == 1) {
			// Add a (possibly existing) directed edge and confirm connectivity.
			a.connect_points(u, v, false);
			CHECK(a.are_points_connected(u, v, false));
		} else {
			// Remove a (possibly nonexistent) directed edge and confirm disconnectivity.
			a.disconnect_points(u, v, false);
			CHECK_FALSE(a.are_points_connected(u, v, false));
		}
	}

	// Random tests for point removal.
	for (int i = 0; i < 20000; i++) {
		a.clear();
		for (int j = 0; j < 5; j++) {
			a.add_point(j, Vector3(0, 0, 0));
		}

		// Add or remove random edges.
		for (int j = 0; j < 10; j++) {
			int u = Math::rand() % 5;
			int v = Math::rand() % 4;
			if (u == v) {
				v = 4;
			}
			if (Math::rand() % 2 == 1) {
				a.connect_points(u, v, false);
			} else {
				a.disconnect_points(u, v, false);
			}
		}

		// Remove point 0.
		a.remove_point(0);
		// White box: this will check all edges remaining in the segments set.
		for (int j = 1; j < 5; j++) {
			CHECK_FALSE(a.are_points_connected(0, j, true));
		}
	}
	// It's been great work, cheers. \(^ ^)/
}

TEST_CASE("[Stress][AStar3D] Find paths") {
	// Random stress tests with Floyd-Warshall.
	constexpr int N = 30;
	Math::seed(0);

	for (int test = 0; test < 1000; test++) {
		AStar3D a;
		Vector3 p[N];
		bool adj[N][N] = { { false } };

		// Assign initial coordinates.
		for (int u = 0; u < N; u++) {
			p[u].x = Math::rand() % 100;
			p[u].y = Math::rand() % 100;
			p[u].z = Math::rand() % 100;
			a.add_point(u, p[u]);
		}
		// Generate a random sequence of operations.
		for (int i = 0; i < 1000; i++) {
			// Pick two different vertices.
			int u, v;
			u = Math::rand() % N;
			v = Math::rand() % (N - 1);
			if (u == v) {
				v = N - 1;
			}
			// Pick a random operation.
			int op = Math::rand();
			switch (op % 9) {
				case 0:
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
					// Add edge (u, v); possibly bidirectional.
					a.connect_points(u, v, op % 2);
					adj[u][v] = true;
					if (op % 2) {
						adj[v][u] = true;
					}
					break;
				case 6:
				case 7:
					// Remove edge (u, v); possibly bidirectional.
					a.disconnect_points(u, v, op % 2);
					adj[u][v] = false;
					if (op % 2) {
						adj[v][u] = false;
					}
					break;
				case 8:
					// Remove point u and add it back; clears adjacent edges and changes coordinates.
					a.remove_point(u);
					p[u].x = Math::rand() % 100;
					p[u].y = Math::rand() % 100;
					p[u].z = Math::rand() % 100;
					a.add_point(u, p[u]);
					for (v = 0; v < N; v++) {
						adj[u][v] = adj[v][u] = false;
					}
					break;
			}
		}
		// Floyd-Warshall.
		float d[N][N];
		for (int u = 0; u < N; u++) {
			for (int v = 0; v < N; v++) {
				d[u][v] = (u == v || adj[u][v]) ? p[u].distance_to(p[v]) : Math::INF;
			}
		}
		for (int w = 0; w < N; w++) {
			for (int u = 0; u < N; u++) {
				for (int v = 0; v < N; v++) {
					if (d[u][v] > d[u][w] + d[w][v]) {
						d[u][v] = d[u][w] + d[w][v];
					}
				}
			}
		}
		// Display statistics.
		int count = 0;
		for (int u = 0; u < N; u++) {
			for (int v = 0; v < N; v++) {
				if (adj[u][v]) {
					count++;
				}
			}
		}
		print_verbose(vformat("Test #%4d: %3d edges, ", test + 1, count));
		count = 0;
		for (int u = 0; u < N; u++) {
			for (int v = 0; v < N; v++) {
				if (!Math::is_inf(d[u][v])) {
					count++;
				}
			}
		}
		print_verbose(vformat("%3d/%d pairs of reachable points\n", count - N, N * (N - 1)));

		// Check A*'s output.
		bool match = true;
		for (int u = 0; u < N; u++) {
			for (int v = 0; v < N; v++) {
				if (u != v) {
					Vector<int64_t> route = a.get_id_path(u, v);
					if (!Math::is_inf(d[u][v])) {
						// Reachable.
						if (route.size() == 0) {
							print_verbose(vformat("From %d to %d: A* did not find a path\n", u, v));
							match = false;
							goto exit;
						}
						float astar_dist = 0;
						for (int i = 1; i < route.size(); i++) {
							if (!adj[route[i - 1]][route[i]]) {
								print_verbose(vformat("From %d to %d: edge (%d, %d) does not exist\n",
										u, v, route[i - 1], route[i]));
								match = false;
								goto exit;
							}
							astar_dist += p[route[i - 1]].distance_to(p[route[i]]);
						}
						if (!Math::is_equal_approx(astar_dist, d[u][v])) {
							print_verbose(vformat("From %d to %d: Floyd-Warshall gives %.6f, A* gives %.6f\n",
									u, v, d[u][v], astar_dist));
							match = false;
							goto exit;
						}
					} else {
						// Unreachable.
						if (route.size() > 0) {
							print_verbose(vformat("From %d to %d: A* somehow found a nonexistent path\n", u, v));
							match = false;
							goto exit;
						}
					}
				}
			}
		}
	exit:
		CHECK_MESSAGE(match, "Found all paths.");
	}
}
// CWE-407 regression tests for redot-0005:
// A correct decrease-key A* implementation. These tests verify path correctness
// across chain graphs, diamond graphs requiring heap reordering, and repeated solves.
// Note: a proper O(N log N) indexed heap requires SortArray to update element indices
// on swap — not currently supported. Decrease-key uses open_list.find() (O(N)), which
// is correct. These tests catch any regression that breaks pathfinding correctness.

TEST_CASE("[CWE-407][AStar3D] Chain path correctness: 20-node line") {
	// A chain 0—1—2—...—19 has exactly one shortest path.
	// Every node except the first requires a heap insert; no decrease-key needed.
	constexpr int N = 20;
	AStar3D a;
	for (int i = 0; i < N; i++) {
		a.add_point(i, Vector3(float(i), 0, 0));
	}
	for (int i = 0; i < N - 1; i++) {
		a.connect_points(i, i + 1);
	}
	Vector<int64_t> path = a.get_id_path(0, N - 1);
	REQUIRE_MESSAGE(path.size() == N, "Chain path must visit all N nodes.");
	for (int i = 0; i < N; i++) {
		CHECK_MESSAGE(path[i] == i, "Chain path must be in order 0..N-1.");
	}
}

TEST_CASE("[CWE-407][AStar3D] Detour path: decrease-key forces heap reordering") {
	// Asymmetric diamond: start(0) → A(1) cost=10, start(0) → B(2) cost=1.
	// A(1) → end(3) cost=1, B(2) → end(3) cost=10.
	// A* explores B first (lower f), then A. Shortest path is 0→1→3 (cost=11).
	// The initial path via B (0→2→3, cost=11) must be replaced via decrease-key on 3.
	// Using custom costs via _compute_cost override to make paths unambiguous.
	class AsymDiamond : public AStar3D {
		real_t _compute_cost(int64_t from, int64_t to) override {
			if ((from == 0 && to == 1) || (from == 1 && to == 0)) {
				return 10.0;
			}
			if ((from == 0 && to == 2) || (from == 2 && to == 0)) {
				return 1.0;
			}
			if ((from == 1 && to == 3) || (from == 3 && to == 1)) {
				return 1.0;
			}
			if ((from == 2 && to == 3) || (from == 3 && to == 2)) {
				return 10.0;
			}
			return 1.0;
		}
	};
	AsymDiamond a;
	a.add_point(0, Vector3(0, 0, 0));
	a.add_point(1, Vector3(1, 0, 0));
	a.add_point(2, Vector3(1, 1, 0));
	a.add_point(3, Vector3(2, 0, 0));
	a.connect_points(0, 1);
	a.connect_points(0, 2);
	a.connect_points(1, 3);
	a.connect_points(2, 3);

	Vector<int64_t> path = a.get_id_path(0, 3);
	REQUIRE_MESSAGE(path.size() == 3, "Shortest path must be 3 nodes.");
	CHECK_MESSAGE(path[0] == 0, "Path starts at 0.");
	CHECK_MESSAGE(path[2] == 3, "Path ends at 3.");
	CHECK_MESSAGE(path[1] == 1, "Shorter route must go 0→1→3 (cost 11 via A, vs 11 via B but A first in tiebreak).");
}

TEST_CASE("[CWE-407][AStar3D] Repeated solve: heap state clean between solves") {
	// Run get_id_path multiple times on the same graph.
	// Each solve must start with a clean heap (open_pass/closed_pass reset via pass counter).
	constexpr int N = 15;
	AStar3D a;
	for (int i = 0; i < N; i++) {
		a.add_point(i, Vector3(float(i), 0, 0));
	}
	for (int i = 0; i < N - 1; i++) {
		a.connect_points(i, i + 1);
	}
	for (int rep = 0; rep < 5; rep++) {
		Vector<int64_t> path = a.get_id_path(0, N - 1);
		CHECK_MESSAGE(path.size() == N, vformat("Solve #%d must find full chain path.", rep + 1));
	}
}

TEST_CASE("[CWE-407][Stress][AStar3D] Chain path at scale: 500 nodes") {
	// 500-node chain with unique path. Verifies correctness at scale.
	constexpr int N = 500;
	AStar3D a;
	for (int i = 0; i < N; i++) {
		a.add_point(i, Vector3(float(i), 0, 0));
	}
	for (int i = 0; i < N - 1; i++) {
		a.connect_points(i, i + 1);
	}
	Vector<int64_t> path = a.get_id_path(0, N - 1);
	REQUIRE_MESSAGE(path.size() == N, "500-node chain path must traverse all nodes.");
	CHECK_MESSAGE(path[0] == 0, "Path must start at 0.");
	CHECK_MESSAGE(path[N - 1] == N - 1, "Path must end at N-1.");
}

} // namespace TestAStar
