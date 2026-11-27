package nre;

/** Run in its own JVM by NreHeadlessTest: calls createModel without setRenderConfig/init. */
final class NotInitializedProbe {
    public static void main(String[] args) {
        try {
            NRE.createModel(args[0]);
            System.out.println("CODE=OK");
        } catch (NreException e) {
            System.out.println("CODE=" + e.code());
            System.out.println("MSG=" + e.nreMessage());
        }
    }
}
