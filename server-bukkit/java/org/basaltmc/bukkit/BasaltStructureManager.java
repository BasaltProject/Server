package org.basaltmc.bukkit;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.NamespacedKey;
import org.bukkit.structure.Structure;
import org.bukkit.structure.StructureManager;

final class BasaltStructureManager implements StructureManager {
    private final Map<NamespacedKey, Structure> registered = new LinkedHashMap<>();

    private static String id(NamespacedKey key) {
        return key.toString();
    }

    private static BasaltTemplate template(Structure structure) {
        if (!(structure instanceof BasaltTemplate own)) {
            throw new IllegalArgumentException("Only structures this server made can be used");
        }
        return own;
    }

    @Override
    public synchronized Map<NamespacedKey, Structure> getStructures() {
        return java.util.Collections.unmodifiableMap(new LinkedHashMap<>(registered));
    }

    @Override
    public synchronized Structure getStructure(NamespacedKey key) {
        return registered.get(key);
    }

    @Override
    public synchronized Structure registerStructure(NamespacedKey key, Structure structure) {
        BasaltTemplate own = template(structure);
        LeftoverNatives.structureStore(id(key), own.bytes(), false);
        return registered.put(key, structure);
    }

    @Override
    public synchronized Structure unregisterStructure(NamespacedKey key) {
        Structure removed = registered.remove(key);
        if (removed != null) {
            LeftoverNatives.structureRemove(id(key), false);
        }
        return removed;
    }

    @Override
    public synchronized Structure loadStructure(NamespacedKey key, boolean register) {
        byte[] bytes = LeftoverNatives.structureRead(id(key));
        if (bytes == null) {
            return null;
        }
        BasaltTemplate loaded = new BasaltTemplate(Nbt.read(bytes));
        if (register) {
            registered.put(key, loaded);
        }
        return loaded;
    }

    @Override
    public Structure loadStructure(NamespacedKey key) {
        return loadStructure(key, true);
    }

    @Override
    public synchronized void saveStructure(NamespacedKey key) {
        Structure structure = registered.get(key);
        if (structure == null) {
            throw new IllegalArgumentException("The structure " + key + " is not registered");
        }
        if (!LeftoverNatives.structureStore(id(key), template(structure).bytes(), true)) {
            throw new IllegalStateException("The structure " + key + " could not be saved");
        }
    }

    @Override
    public void saveStructure(NamespacedKey key, Structure structure) throws IOException {
        if (!LeftoverNatives.structureStore(id(key), template(structure).bytes(), true)) {
            throw new IOException("The structure " + key + " could not be saved");
        }
    }

    @Override
    public void deleteStructure(NamespacedKey key) throws IOException {
        deleteStructure(key, true);
    }

    @Override
    public synchronized void deleteStructure(NamespacedKey key, boolean unregister) throws IOException {
        if (unregister) {
            registered.remove(key);
        }
        File file = getStructureFile(key);
        boolean existed = file.exists();
        LeftoverNatives.structureRemove(id(key), true);
        if (existed && file.exists()) {
            throw new IOException("The structure file " + file + " could not be deleted");
        }
    }

    @Override
    public File getStructureFile(NamespacedKey key) {
        return new File(LeftoverNatives.structureFile(id(key)));
    }

    @Override
    public Structure loadStructure(File file) throws IOException {
        try (InputStream in = new java.io.FileInputStream(file)) {
            return loadStructure(in);
        }
    }

    @Override
    public Structure loadStructure(InputStream input) throws IOException {
        byte[] plain = new GZIPInputStream(input).readAllBytes();
        Map<String, Object> read = Nbt.readNamed(plain);
        if (read == null) {
            throw new IOException("Not a structure file");
        }
        int version = Nbt.integer(read, "DataVersion", 0);
        if (version > 0 && version < WorldNatives.dataVersion()) {
            byte[] fixed = WorldNatives.fixNbt("structure", Nbt.write(read), version);
            if (fixed != null) {
                read = Nbt.read(fixed);
            }
        }
        return new BasaltTemplate(read);
    }

    @Override
    public void saveStructure(File file, Structure structure) throws IOException {
        File parent = file.getParentFile();
        if (parent != null) {
            parent.mkdirs();
        }
        try (OutputStream out = new java.io.FileOutputStream(file)) {
            saveStructure(out, structure);
        }
    }

    @Override
    public void saveStructure(OutputStream output, Structure structure) throws IOException {
        Map<String, Object> data = Nbt.deepCopy(template(structure).data());
        data.put("DataVersion", WorldNatives.dataVersion());
        GZIPOutputStream gzip = new GZIPOutputStream(output);
        gzip.write(Nbt.writeNamed(data));
        gzip.finish();
    }

    @Override
    public Structure createStructure() {
        return BasaltTemplate.empty();
    }

    @Override
    public Structure copy(Structure structure) {
        return template(structure).copy();
    }
}
