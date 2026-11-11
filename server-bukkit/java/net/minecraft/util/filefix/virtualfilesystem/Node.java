// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.virtualfilesystem;

abstract sealed class Node permits FileNode, DirectoryNode {
    protected DirectoryNode parent;
    protected CopyOnWriteFSPath path;

    protected Node(CopyOnWriteFSPath path) {
        setPath(path);
    }

    public String name() {
        CopyOnWriteFSPath fileName = path.getFileName();
        return fileName == null ? null : fileName.toString();
    }

    protected void setParent(DirectoryNode parent) {
        this.parent = parent;
    }

    protected void setPath(CopyOnWriteFSPath path) {
        this.path = path.normalize().toAbsolutePath();
    }

    public CopyOnWriteFSPath path() {
        return path;
    }
}
