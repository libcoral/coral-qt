local env = require "testkit.env"

local qt = require "qt"

local testWidget = qt.new( "QWidget" )

function testEventNotification()
	local hit = false
	testWidget.onClose = function() hit = true end
	testWidget.visible = true
	testWidget:invoke( "close()" )
	env.ASSERT_TRUE( hit, "The event was not called" )
end
