// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.Portal;
import net.minecraft.world.level.portal.TeleportTransition;
public class PortalProcessor {
    private final Portal portal;
    private BlockPos entryPosition;
    private int portalTime;
    private boolean insidePortalThisTick = true;
    public PortalProcessor(Portal portal, BlockPos portalEntryPosition) {
        this.portal = portal;
        this.entryPosition = portalEntryPosition;
    }
    public boolean processPortalTeleportation(ServerLevel serverLevel, Entity entity, boolean allowedToTeleport) {
        if (!insidePortalThisTick) {
            portalTime = Math.max(portalTime - 4, 0);
            return false;
        }
        insidePortalThisTick = false;
        int elapsed = portalTime++;
        return allowedToTeleport && elapsed >= portal.getPortalTransitionTime(serverLevel, entity);
    }
    public TeleportTransition getPortalDestination(ServerLevel serverLevel, Entity entity) {
        return portal.getPortalDestination(serverLevel, entity, entryPosition);
    }
    public Portal.Transition getPortalLocalTransition() {
        return portal.getLocalTransition();
    }
    public boolean hasExpired() {
        return portalTime <= 0;
    }
    public BlockPos getEntryPosition() {
        return entryPosition;
    }
    public void updateEntryPosition(BlockPos entryPosition) {
        this.entryPosition = entryPosition;
    }
    public int getPortalTime() {
        return portalTime;
    }
    public boolean isInsidePortalThisTick() {
        return insidePortalThisTick;
    }
    public void setAsInsidePortalThisTick(boolean insidePortal) {
        this.insidePortalThisTick = insidePortal;
    }
    public boolean isSamePortal(Portal portal) {
        return this.portal == portal;
    }
}
