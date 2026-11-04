// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;

import com.mojang.datafixers.DataFixer;
import java.nio.file.Path;
import java.util.Map;
import net.minecraft.core.HolderLookup;
import java.io.IOException;
import java.util.concurrent.CompletableFuture;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.datafix.DataFixTypes;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;
public class SavedDataStorage implements AutoCloseable {
    public Map cache;
    public SavedDataStorage(Path path, DataFixer dataFixer, HolderLookup.Provider provider) {
    }
    public <T extends SavedData> T computeIfAbsent(SavedDataType<T> type) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.SavedDataStorage#computeIfAbsent");
    }
    public <T extends SavedData> T get(SavedDataType<T> type) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.SavedDataStorage#get");
    }
    public <T extends SavedData> void set(SavedDataType<T> type, T data) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.SavedDataStorage#set");
    }
    public CompoundTag readTagFromDisk(Path dataFile, DataFixTypes type, int newVersion) throws IOException {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.SavedDataStorage#readTagFromDisk");
    }
    public CompletableFuture<?> scheduleSave() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.SavedDataStorage#scheduleSave");
    }
    public void saveAndJoin() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.SavedDataStorage#saveAndJoin");
    }
    public void close() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.SavedDataStorage#close");
    }
}
