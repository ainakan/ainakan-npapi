[CCode (cprefix = "NPAinakan", lower_case_cprefix = "npainakan_")]
namespace NPAinakan {
	[CCode (cheader_filename = "npainakan-object.h")]
	public abstract class Object : GLib.Object {
		[CCode (has_construct_function = false)]
		protected Object ();

		protected abstract async void destroy ();
	}
}
