package net.minecraft.sounds;

import java.util.Optional;
import net.minecraft.resources.Identifier;
public record SoundEvent(Identifier location, Optional fixedRange) {
}
