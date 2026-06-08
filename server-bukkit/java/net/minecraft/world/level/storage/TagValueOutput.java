package net.minecraft.world.level.storage;

import net.minecraft.core.HolderLookup;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.ProblemReporter;
import org.basaltmc.bukkit.Unsupported;
public class TagValueOutput implements ValueOutput {
    protected TagValueOutput() {
    }
    public CompoundTag buildResult() {
        throw Unsupported.hit("net.minecraft.world.level.storage.TagValueOutput.buildResult");
    }
    public static TagValueOutput createWithContext(ProblemReporter problemReporter, HolderLookup.Provider provider) {
        throw Unsupported.hit("net.minecraft.world.level.storage.TagValueOutput.createWithContext");
    }
}
