package net.minecraft.world.level.storage;

import com.mojang.datafixers.DataFixer;
import java.nio.file.Path;
import java.util.Map;
import net.minecraft.core.HolderLookup;
public class SavedDataStorage {
    public Map cache;
    public SavedDataStorage(Path path, DataFixer dataFixer, HolderLookup.Provider provider) {
    }
}
