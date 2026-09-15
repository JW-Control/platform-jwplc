const fs = require('fs');
let code = fs.readFileSync('app.js', 'utf8');

code = code.replace(
  "    selectedFieldKey = field.key;\n    syncInputsFromState();",
  "    selectedFieldKey = field.key;\n    selectedTool = 'pointer';\n    syncInputsFromState();"
);

fs.writeFileSync('app.js', code);
console.log('Update successful');
