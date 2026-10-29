# Companion Smart Jump Link (UE 5.6)

`CompanionJumpNavLink` is a reusable native Smart Nav Link. It executes a normal
character jump between its endpoints and resumes the existing follow request only
after landing near the destination. It does not alter existing maps or links.

## Place One Test Link

1. Close Unreal, build the updated C++ project, and reopen it with UE 5.6.
2. Open the blockout. Press P and check that the lower and upper surfaces have green
   navigation. Start with a low, wide platform and a clear landing area.
3. Find **CompanionJumpNavLink** under C++ Classes / mechanic_testing in the Content
   Browser, or search **Companion Jump Nav Link** in Place Actors / All Classes.
   Drag an instance into the level. A Blueprint child is optional, not required.
4. Select it. Under **Jump Link**, edit **Start Point** and **End Point**, using
   their viewport widgets or XYZ fields. These coordinates are relative to the
   actor. Put one on the lower green NavMesh and the other on the upper green
   NavMesh, set back from the edge far enough for the character's capsule to fit.
5. Leave **Direction = Both Ways**. This class automatically enables its Smart Link
   and clears simple Point Links, so no Copy End Points button or Blueprint jump
   graph is needed. Edit Start Point / End Point, not the inherited Point Links.
6. Remove or move aside any old link connecting the same endpoints for this test.
   Temporarily disable automatic Generate Nav Links and rebuild if generated links
   let the AI take a different route. Do not delete unrelated navigation or assets.
7. Choose **Build > Build Paths** and save the map.
8. Press Play. Jump onto the platform, then move several metres away from the link's
   upper landing point. The follower should approach the lower endpoint, jump,
   land, and continue following. Test the return direction and repeat after Tab.

This is a point-to-point link, not coverage for an entire circular ledge. Duplicate
it for other intended jump routes. Endpoints must be connected to usable NavMesh;
merely placing them on a visible mesh does not guarantee navigation connectivity.

## Jump Limits and Behaviour

- The launch uses the character's existing Jump Z Velocity, Gravity Scale, capsule,
  and Max Walk Speed. No extra jump strength or double jump is granted to the AI.
- Destinations more than 5 cm below the character's feet use a walk-off and
  fall, with no upward launch. Place the lower endpoint close to the ledge: this
  does not grant extra horizontal reach or let the character cross a wide gap.
- The landing surface is checked. For jumps, Unreal predicts the arc and the full
  capsule is swept along the samples to reject blocked routes. Drops use normal
  walking/falling collision and a bounded horizontal speed after leaving the edge.
- Normal following is paused during the jump. Air steering and falling drag are
  temporarily suppressed for the calculated trajectory, then restored afterward.
- Landing must occur near the intended destination. Timeout, possession changes,
  aborts, or link removal release the traversal and restore movement settings.
  There is no teleporting, climbing, or special jump animation added here.
- An already-reached link endpoint is released without jumping again when path
  following revisits the completed segment. Reverse traversal remains available.

## Debug and Test

Enable **Draw Jump Debug** on the link to see sampled clear/blocked trajectory
segments when the AI attempts it. Open Output Log and search for **Jump link** for
rejection reasons: unreachable height/distance, blocked arc, invalid landing,
landing outside tolerance, or timeout. Warnings are throttled to avoid log spam.

If no attempt or warning appears, first check the navigation connection and whether
another route/link is being chosen. Debug drawing does not make a rejected link
reachable. Do not increase character jump strength merely to bypass a blocked arc.

Test in UE 5.6: both characters, both directions, walking animation afterward,
switching during the jump, an occupied landing area, low ceilings, blocked paths,
and Stop/Play. Changing the map requires saving it on the university PC; the Mac's
UE 5.3 must not open/resave this 5.6 map.

## Verification

The native code is checked separately in an isolated UE 5.3 compatibility project;
the real UE 5.6 map, NavMesh connectivity, visuals, and runtime routing still need
the PC test above. Planning/traversal automation uses primitive test geometry and
synthetic paths, not a baked copy of the blockout NavMesh.

UnrealHeaderTool and the Development Editor build passed. All nine MechanicTesting
automation tests passed with zero warnings or failures. The added tests exercise
reachable jumps for both roles, physical traversal up and down, blocked and
unreachable routes, landing/resuming, switching possession, timeout, and cleanup.
Drop regression checks cover no upward velocity, repeated endpoint callbacks
without another launch, and reverse traversal after arrival.
