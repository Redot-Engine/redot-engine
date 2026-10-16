// Node-level physics smoke test, ported from the module's smoke-test example.
//
// Spawns a RigidBody3D (box) above a StaticBody3D floor as real scene nodes,
// lets the engine drive physics, and asserts the body falls and then settles
// resting on the floor. Unlike the rest of the suite (server-level RIDs), this
// exercises the full stack the way a game does: nodes -> PhysicsServer3D ->
// the PhysX backend.

using System;
using System.Collections;
using Godot;

namespace PhysxTestProject.Tests;

public static class SmokeTests
{
	const int MaxTicks = 600;      // 10 s at 60 Hz
	const float SettleEps = 0.01f;
	const float FloorTopY = 0.0f;
	const float BoxHalf = 0.5f;
	const float StartY = 5.0f;

	public static void Register(SuiteBuilder sb)
	{
		sb.Add("PHYSX-SMOKE-001", "node-level drop: box falls from 5 m and settles on the floor", FallAndSettle);
	}

	static IEnumerator FallAndSettle()
	{
		var tree = Engine.GetMainLoop() as SceneTree;
		Assert.Require(tree != null, "running inside a SceneTree");
		var root = tree.Root;

		var floorBody = new StaticBody3D();
		var floorCol = new CollisionShape3D();
		var floorShape = new BoxShape3D { Size = new Vector3(10, 1, 10) };
		floorCol.Shape = floorShape;
		floorBody.AddChild(floorCol);
		floorBody.Position = new Vector3(0, FloorTopY - 0.5f, 0);
		root.AddChild(floorBody);

		var box = new RigidBody3D();
		var boxCol = new CollisionShape3D();
		var boxShape = new BoxShape3D { Size = new Vector3(BoxHalf * 2, BoxHalf * 2, BoxHalf * 2) };
		boxCol.Shape = boxShape;
		box.AddChild(boxCol);
		box.Position = new Vector3(0, StartY, 0);
		root.AddChild(box);

		try
		{
			// One process frame so the nodes enter the tree and the physics
			// server picks them up.
			yield return Wait.Frames(2);
			Log.Info($"[smoke] physics/3d/physics_engine = {ProjectSettings.GetSettingWithOverride("physics/3d/physics_engine")}");
			Log.Info($"[smoke] box y right after add_child = {box.GlobalPosition.Y:F3}");

			float yPrev = StartY;
			float minY = StartY;
			int settledAt = -1;
			int ticks = 0;

			while (ticks < MaxTicks)
			{
				yield return Wait.Frame();
				ticks++;
				float y = box.GlobalPosition.Y;
				minY = Math.Min(minY, y);
				if (ticks <= 5 || ticks % 60 == 0)
				{
					Log.Info($"[smoke] tick {ticks}  y={y:F3}");
				}
				if (ticks > 2 && Math.Abs(y - yPrev) < SettleEps && y < StartY - 0.5f)
				{
					settledAt = ticks;
					break;
				}
				yPrev = y;
			}

			float yEnd = box.GlobalPosition.Y;
			float expectedRest = FloorTopY + BoxHalf;
			Log.Info($"[smoke] y: start={StartY:F3} min={minY:F3} end={yEnd:F3}  settled_tick={settledAt}  expected_rest~{expectedRest:F3}  ticks={ticks}");

			Assert.Expect(yEnd < StartY - 1.0f, "box did not fall");
			Assert.Expect(settledAt > 0, $"box never settled within {MaxTicks} ticks");
			if (settledAt > 0)
			{
				Assert.ExpectNear(yEnd, expectedRest, 0.2f, "rest height");
			}
		}
		finally
		{
			// Framework guarantees finally runs even on abort/timeout.
			root.RemoveChild(box);
			box.QueueFree();
			root.RemoveChild(floorBody);
			floorBody.QueueFree();
		}
	}
}
