package nre;

/** Run in its own JVM by NreHeadlessTest: loads the library from NRE_LIB_DIR and reports whether it was rejected. */
final class VersionProbe {
    public static void main(String[] args) {
        try {
            System.out.println("LOADED " + NRE.getVersion());
        } catch (UnsatisfiedLinkError | ExceptionInInitializerError e) {
            Throwable t = e instanceof ExceptionInInitializerError i && i.getCause() != null ? i.getCause() : e;
            System.out.println("REJECTED " + t.getMessage());
        }
    }
}
