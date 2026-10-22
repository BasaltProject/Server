package net.minecraft.server.level;

import net.minecraft.world.entity.HumanoidArm;
import net.minecraft.world.entity.player.ChatVisiblity;
public record ClientInformation(String language, int viewDistance, ChatVisiblity chatVisibility, boolean chatColors, int modelCustomisation, HumanoidArm mainHand, boolean textFilteringEnabled, boolean allowsListing, ParticleStatus particleStatus) {
}
