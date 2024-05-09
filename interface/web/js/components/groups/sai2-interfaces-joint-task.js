class Sai2InterfacesJointTask extends HTMLElement {
	constructor() {
		super();
		this.robotName = this.getAttribute('robotName');
		this.controllerName = this.getAttribute('controllerName');
		this.taskName = this.getAttribute('taskName');
		this.redisPrefix = "sai2::interfaces::controller::";
		this.joint_lower_limits = this.getAttribute('lowerJointLimits');
		this.joint_upper_limits = this.getAttribute('upperJointLimits');
		if (this.getAttribute('redisPrefix')) {
			this.redisPrefix = this.getAttribute('redisPrefix') + "::controller::";
		}
		this.redisPrefix += this.robotName + "::" + this.controllerName + "::" + this.taskName;

		// Fetch the HTML template
		fetch('html/component_groups_templates/sai2-interfaces-joint-task.html')
			.then(response => response.text())
			.then(template => {
				let replacedHTML = template.replaceAll('{{_prefix_}}', this.redisPrefix);

				if (this.joint_lower_limits) {
					replacedHTML = replacedHTML.replaceAll('{{_joint_lower_limits_}}', this.joint_lower_limits);
				} else {
					replacedHTML = replacedHTML.replaceAll('{{_joint_lower_limits_}}', "-3.14");
				}

				if (this.joint_upper_limits) {
					replacedHTML = replacedHTML.replaceAll('{{_joint_upper_limits_}}', this.joint_upper_limits);
				} else {
					replacedHTML = replacedHTML.replaceAll('{{_joint_upper_limits_}}', "3.14");
				}

				this.innerHTML = replacedHTML;
			});
	}
}

// Define the custom element
customElements.define('sai2-interfaces-joint-task', Sai2InterfacesJointTask);
