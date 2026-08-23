package org.basaltmc.bukkit.item;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public final class Nbt {
    private static final int kEnd = 0;
    private static final int kByte = 1;
    private static final int kShort = 2;
    private static final int kInt = 3;
    private static final int kLong = 4;
    private static final int kFloat = 5;
    private static final int kDouble = 6;
    private static final int kByteArray = 7;
    private static final int kString = 8;
    private static final int kList = 9;
    private static final int kCompound = 10;
    private static final int kIntArray = 11;
    private static final int kLongArray = 12;
    private static final int kMaxDepth = 512;

    private Nbt() {}

    public static Map<String, Object> compound() {
        return new LinkedHashMap<>();
    }

    @SuppressWarnings("unchecked")
    public static Map<String, Object> deepCopy(Map<String, Object> source) {
        return (Map<String, Object>) copy(source);
    }

    @SuppressWarnings("unchecked")
    public static Object copy(Object value) {
        if (value instanceof Map<?, ?> map) {
            Map<String, Object> out = new LinkedHashMap<>();
            for (Map.Entry<?, ?> entry : map.entrySet()) {
                out.put((String) entry.getKey(), copy(entry.getValue()));
            }
            return out;
        }
        if (value instanceof List<?> list) {
            List<Object> out = new ArrayList<>(list.size());
            for (Object element : list) {
                out.add(copy(element));
            }
            return out;
        }
        if (value instanceof byte[] bytes) {
            return bytes.clone();
        }
        if (value instanceof int[] ints) {
            return ints.clone();
        }
        if (value instanceof long[] longs) {
            return longs.clone();
        }
        return value;
    }

    public static boolean same(Object a, Object b) {
        if (a == b) {
            return true;
        }
        if (a == null || b == null) {
            return false;
        }
        if (a instanceof Map<?, ?> left && b instanceof Map<?, ?> right) {
            if (left.size() != right.size()) {
                return false;
            }
            for (Map.Entry<?, ?> entry : left.entrySet()) {
                if (!right.containsKey(entry.getKey()) || !same(entry.getValue(), right.get(entry.getKey()))) {
                    return false;
                }
            }
            return true;
        }
        if (a instanceof List<?> left && b instanceof List<?> right) {
            if (left.size() != right.size()) {
                return false;
            }
            for (int at = 0; at < left.size(); at++) {
                if (!same(left.get(at), right.get(at))) {
                    return false;
                }
            }
            return true;
        }
        if (a instanceof byte[] left && b instanceof byte[] right) {
            return java.util.Arrays.equals(left, right);
        }
        if (a instanceof int[] left && b instanceof int[] right) {
            return java.util.Arrays.equals(left, right);
        }
        if (a instanceof long[] left && b instanceof long[] right) {
            return java.util.Arrays.equals(left, right);
        }
        return a.equals(b);
    }

    public static int hash(Object value) {
        if (value instanceof Map<?, ?> map) {
            int h = 0;
            for (Map.Entry<?, ?> entry : map.entrySet()) {
                h += entry.getKey().hashCode() ^ hash(entry.getValue());
            }
            return h;
        }
        if (value instanceof List<?> list) {
            int h = 1;
            for (Object element : list) {
                h = h * 31 + hash(element);
            }
            return h;
        }
        if (value instanceof byte[] bytes) {
            return java.util.Arrays.hashCode(bytes);
        }
        if (value instanceof int[] ints) {
            return java.util.Arrays.hashCode(ints);
        }
        if (value instanceof long[] longs) {
            return java.util.Arrays.hashCode(longs);
        }
        return value == null ? 0 : value.hashCode();
    }

    public static byte[] write(Map<String, Object> root) {
        try {
            ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(bytes);
            out.writeByte(kCompound);
            writePayload(out, root);
            return bytes.toByteArray();
        } catch (IOException e) {
            throw new IllegalStateException(e);
        }
    }

    @SuppressWarnings("unchecked")
    public static Map<String, Object> read(byte[] data) {
        if (data == null || data.length == 0) {
            return null;
        }
        try {
            DataInputStream in = new DataInputStream(new ByteArrayInputStream(data));
            if (in.readByte() != kCompound) {
                return null;
            }
            return (Map<String, Object>) readPayload(in, kCompound);
        } catch (IOException e) {
            throw new IllegalStateException(e);
        }
    }

    public static byte[] writeNamed(Map<String, Object> root) {
        try {
            ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(bytes);
            out.writeByte(kCompound);
            writeString(out, "");
            writePayload(out, root);
            return bytes.toByteArray();
        } catch (IOException e) {
            throw new IllegalStateException(e);
        }
    }

    @SuppressWarnings("unchecked")
    public static Map<String, Object> readNamed(byte[] data) {
        if (data == null || data.length == 0) {
            return null;
        }
        try {
            DataInputStream in = new DataInputStream(new ByteArrayInputStream(data));
            if (in.readByte() != kCompound) {
                return null;
            }
            readString(in);
            return (Map<String, Object>) readPayload(in, kCompound);
        } catch (IOException e) {
            throw new IllegalStateException(e);
        }
    }

    private static int typeOf(Object value) {
        if (value instanceof Byte || value instanceof Boolean) {
            return kByte;
        }
        if (value instanceof Short) {
            return kShort;
        }
        if (value instanceof Integer) {
            return kInt;
        }
        if (value instanceof Long) {
            return kLong;
        }
        if (value instanceof Float) {
            return kFloat;
        }
        if (value instanceof Double) {
            return kDouble;
        }
        if (value instanceof byte[]) {
            return kByteArray;
        }
        if (value instanceof String) {
            return kString;
        }
        if (value instanceof List) {
            return kList;
        }
        if (value instanceof Map) {
            return kCompound;
        }
        if (value instanceof int[]) {
            return kIntArray;
        }
        if (value instanceof long[]) {
            return kLongArray;
        }
        throw new IllegalArgumentException("not an NBT value: " + value);
    }

    private static void writePayload(DataOutputStream out, Object value) throws IOException {
        switch (typeOf(value)) {
            case kByte:
                out.writeByte(value instanceof Boolean flag ? (flag ? 1 : 0) : (Byte) value);
                break;
            case kShort:
                out.writeShort((Short) value);
                break;
            case kInt:
                out.writeInt((Integer) value);
                break;
            case kLong:
                out.writeLong((Long) value);
                break;
            case kFloat:
                out.writeFloat((Float) value);
                break;
            case kDouble:
                out.writeDouble((Double) value);
                break;
            case kByteArray: {
                byte[] bytes = (byte[]) value;
                out.writeInt(bytes.length);
                out.write(bytes);
                break;
            }
            case kString:
                writeString(out, (String) value);
                break;
            case kList: {
                List<?> list = (List<?>) value;
                int type = list.isEmpty() ? kEnd : typeOf(list.get(0));
                out.writeByte(type);
                out.writeInt(list.size());
                for (Object element : list) {
                    writePayload(out, element);
                }
                break;
            }
            case kCompound: {
                for (Map.Entry<?, ?> entry : ((Map<?, ?>) value).entrySet()) {
                    if (entry.getValue() == null) {
                        continue;
                    }
                    out.writeByte(typeOf(entry.getValue()));
                    writeString(out, (String) entry.getKey());
                    writePayload(out, entry.getValue());
                }
                out.writeByte(kEnd);
                break;
            }
            case kIntArray: {
                int[] ints = (int[]) value;
                out.writeInt(ints.length);
                for (int element : ints) {
                    out.writeInt(element);
                }
                break;
            }
            default: {
                long[] longs = (long[]) value;
                out.writeInt(longs.length);
                for (long element : longs) {
                    out.writeLong(element);
                }
                break;
            }
        }
    }

    private static void writeString(DataOutputStream out, String text) throws IOException {
        byte[] encoded = text.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        out.writeShort(encoded.length);
        out.write(encoded);
    }

    private static String readString(DataInputStream in) throws IOException {
        byte[] encoded = new byte[in.readUnsignedShort()];
        in.readFully(encoded);
        return new String(encoded, java.nio.charset.StandardCharsets.UTF_8);
    }

    private static int length(DataInputStream in, int unit) throws IOException {
        int size = in.readInt();
        if (size < 0 || (long) size * unit > in.available()) {
            throw new IOException("bad NBT length " + size);
        }
        return size;
    }

    private static Object readPayload(DataInputStream in, int type) throws IOException {
        return readPayload(in, type, 0);
    }

    private static Object readPayload(DataInputStream in, int type, int depth) throws IOException {
        if (depth > kMaxDepth) {
            throw new IOException("NBT nested too deep");
        }
        switch (type) {
            case kByte:
                return in.readByte();
            case kShort:
                return in.readShort();
            case kInt:
                return in.readInt();
            case kLong:
                return in.readLong();
            case kFloat:
                return in.readFloat();
            case kDouble:
                return in.readDouble();
            case kByteArray: {
                byte[] bytes = new byte[length(in, 1)];
                in.readFully(bytes);
                return bytes;
            }
            case kString:
                return readString(in);
            case kList: {
                int elementType = in.readUnsignedByte();
                int size = length(in, elementType == kEnd ? 0 : 1);
                List<Object> list = new ArrayList<>(size);
                for (int at = 0; at < size; at++) {
                    list.add(readPayload(in, elementType, depth + 1));
                }
                return list;
            }
            case kCompound: {
                Map<String, Object> map = new LinkedHashMap<>();
                for (int next = in.readUnsignedByte(); next != kEnd; next = in.readUnsignedByte()) {
                    String key = readString(in);
                    map.put(key, readPayload(in, next, depth + 1));
                }
                return map;
            }
            case kIntArray: {
                int[] ints = new int[length(in, 4)];
                for (int at = 0; at < ints.length; at++) {
                    ints[at] = in.readInt();
                }
                return ints;
            }
            case kLongArray: {
                long[] longs = new long[length(in, 8)];
                for (int at = 0; at < longs.length; at++) {
                    longs[at] = in.readLong();
                }
                return longs;
            }
            default:
                throw new IOException("unknown NBT type " + type);
        }
    }

    @SuppressWarnings("unchecked")
    public static Map<String, Object> map(Object value) {
        return value instanceof Map ? (Map<String, Object>) value : null;
    }

    @SuppressWarnings("unchecked")
    public static List<Object> list(Object value) {
        return value instanceof List ? (List<Object>) value : null;
    }

    public static Map<String, Object> map(Map<String, Object> from, String key) {
        return map(from.get(key));
    }

    public static Map<String, Object> mapOrCreate(Map<String, Object> from, String key) {
        Map<String, Object> found = map(from.get(key));
        if (found == null) {
            found = compound();
            from.put(key, found);
        }
        return found;
    }

    public static List<Object> list(Map<String, Object> from, String key) {
        return list(from.get(key));
    }

    public static String string(Map<String, Object> from, String key, String fallback) {
        return from != null && from.get(key) instanceof String text ? text : fallback;
    }

    public static int integer(Map<String, Object> from, String key, int fallback) {
        return from != null && from.get(key) instanceof Number number ? number.intValue() : fallback;
    }

    public static long longValue(Map<String, Object> from, String key, long fallback) {
        return from != null && from.get(key) instanceof Number number ? number.longValue() : fallback;
    }

    public static double real(Map<String, Object> from, String key, double fallback) {
        return from != null && from.get(key) instanceof Number number ? number.doubleValue() : fallback;
    }

    public static boolean flag(Map<String, Object> from, String key, boolean fallback) {
        if (from == null || !from.containsKey(key)) {
            return fallback;
        }
        Object value = from.get(key);
        if (value instanceof Boolean flag) {
            return flag;
        }
        return value instanceof Number number ? number.intValue() != 0 : fallback;
    }

    public static Byte bool(boolean flag) {
        return (byte) (flag ? 1 : 0);
    }
}
