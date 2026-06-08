package net.minecraft.world.level.storage;

import net.minecraft.core.HolderLookup;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.ProblemReporter;
import org.basaltmc.bukkit.Unsupported;
public class TagValueInput implements ValueInput {
    protected TagValueInput() {
    }
    public static ValueInput create(ProblemReporter problemReporter, HolderLookup.Provider holders, CompoundTag tag) {
        throw Unsupported.hit("net.minecraft.world.level.storage.TagValueInput.create");
    }
}
